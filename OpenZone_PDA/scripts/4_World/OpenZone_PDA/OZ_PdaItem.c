// Предмет-КПК.
//
// Живлення -- рушійне: батарея це просто attachments[] у конфізі, а plugType=1
// плюс attachmentAction=1 змушують рушій самому втикати пристрій у вкладену
// батарею й висмикувати при вилученні. Скриптові лишається возити стан.
//
// ЗАМОК І СЕСІЯ -- РІЗНІ РЕЧІ, і в цьому вся механіка.
//
//   ЗАМОК належить ПРИСТРОЮ. Розблокований КПК працює в будь-чиїх руках --
//   передав сталкер свій відімкнений ПДА, і ним можна користуватись. Сам
//   запирається через N хвилин після того, як його прибрали з рук.
//
//   Щоб ЗМІНИТИ пін, його треба знати. Пристрій не питає, хто ти, він питає
//   старий код -- і автоблокування вимикає будь-хто, у кого КПК у руках.
//
//   СЕСІЯ належить ГРАВЦЕВІ й вирішує ЧИЇ дані видно, а не що можна натиснути.
//   Живе, поки епоха пристрою збігається з епохою гравця в його файлі.
//   «Скинути інші сесії» -- це епоха += 1, після чого решта пристроїв
//   переходять в ОФЛАЙН: не ламаються, а перестають оновлюватись, і їхній
//   знімок замерзає на цій миті.
//
// ПІН перевіряється ЗАВЖДИ на сервері й клієнтові не їде ніколи: інакше
// замок знімався б правкою пам'яті клієнта, тобто його не було б узагалі.

class OZ_PDA_Base : ItemBase
{
    private bool  m_IsOn     = false;
    private float m_Charge01 = 0;

    // --- замок ---
    private string m_Pin        = "";     // порожній рядок = коду немає
    private bool   m_Unlocked   = false;  // стан ПРИСТРОЮ, не гравця
    private bool   m_AutoLock   = true;   // можна вимкнути власником сесії
    private int    m_LeftHandsAt = 0;     // GetGame().GetTime() у мс, 0 = в руках

    // --- сесія ---
    private string m_SessionUid   = "";
    private int    m_SessionEpoch = 0;

    // --- знімок даних ---
    // Поки сесія жива, сервер оновлює цей знімок. Коли гравець скинув сесії
    // з іншого пристрою, епохи розходяться, оновлення припиняються -- і
    // знімок ЗАМЕРЗАЄ. Пристрій не втрачає даних і не стає цеглиною: він
    // стає офлайном. Зламати його можна, але побачити там світ таким, яким
    // він був у мить розлогіну, і ні секундою пізніше.
    //
    // Украдений КПК через це -- капсула часу, а не жива прослушка. Це і є
    // головна причина такої моделі.
    private string m_Snapshot   = "";
    private string m_SnapshotAt = "";

    // --- тік модулів ---
    // Базовий таймер один на пристрій; кожен модуль накопичує свій час і
    // спрацьовує зі своїм періодом. Один таймер замість трьох -- бо таймерів
    // стільки ж, скільки КПК на сервері, а не скільки модулів.
    private ref Timer m_ModuleTimer;
    private ref array<float> m_ModuleAcc;
    private static const float MODULE_TICK_BASE = 0.25;

    // --- лічильник невдалих спроб ---
    private ref array<string> m_FailUid;
    private ref array<int>    m_FailCount;

    static const int PIN_MAX_FAILS = 5;

    void OZ_PDA_Base()
    {
        RegisterNetSyncVariableBool("m_IsOn");
        // (ім'я, мін, макс, точність у знаках після коми). Для 0..1 два знаки
        // -- це один відсоток, дрібніше ніхто не побачить.
        RegisterNetSyncVariableFloat("m_Charge01", 0, 1, 2);
        RegisterNetSyncVariableBool("m_Unlocked");

        m_FailUid   = new array<string>();
        m_FailCount = new array<int>();

        m_ModuleAcc = new array<float>();
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
            m_ModuleAcc.Insert(0);
    }

    // --------------------------------------------------------------- модулі

    override void EEItemAttached(EntityAI item, string slot_name)
    {
        super.EEItemAttached(item, slot_name);

        // Батарея не модуль, і відсіку в неї немає -- але заряд і сама її
        // наявність їдуть на клієнт саме звідси. Без цього вставлена батарея
        // з'являлась би на екрані лише при наступному чужому оновленні.
        if (slot_name == OZ_PdaConst.SLOT_BATTERY && GetGame().IsServer())
            PushState();

        int idx = SlotIndexOf(slot_name);
        if (idx == -1)
            return;

        m_ModuleAcc[idx] = 0;

        OZ_ModuleBehaviour b = OZ_PdaModules.ForClass(item.GetType());
        if (b)
            b.OnAttached(this, idx);
    }

    override void EEItemDetached(EntityAI item, string slot_name)
    {
        super.EEItemDetached(item, slot_name);

        if (slot_name == OZ_PdaConst.SLOT_BATTERY && GetGame().IsServer())
            PushState();

        int idx = SlotIndexOf(slot_name);
        if (idx == -1)
            return;

        // Гасить свої звуки й ефекти мусить сам модуль: КПК за чужим кодом
        // не прибирає й не може знати, що той завів.
        OZ_ModuleBehaviour b = OZ_PdaModules.ForClass(item.GetType());
        if (b)
            b.OnDetached(this, idx);
    }

    private int SlotIndexOf(string slot_name)
    {
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            if (OZ_PdaConst.ModuleSlot(i) == slot_name)
                return i;
        }
        return -1;
    }

    // Тікає і на сервері, і на клієнті: звук детектора мусить чути сам
    // гравець, а це клієнтська справа. Хто саме що робить -- вирішує модуль.
    private void StartModuleTicks()
    {
        if (m_ModuleTimer && m_ModuleTimer.IsRunning())
            return;

        if (!m_ModuleTimer)
            m_ModuleTimer = new Timer(CALL_CATEGORY_SYSTEM);

        m_ModuleTimer.Run(MODULE_TICK_BASE, this, "ModuleTick", NULL, true);
    }

    private void StopModuleTicks()
    {
        if (m_ModuleTimer)
            m_ModuleTimer.Stop();
    }

    void ModuleTick()
    {
        // Вимкнений пристрій нічого не міряє й не пищить.
        if (!m_IsOn)
        {
            StopModuleTicks();
            return;
        }

        Man owner = Man.Cast(GetHierarchyRootPlayer());

        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string cls = OZ_ModuleClass(i);
            if (cls == "")
                continue;

            OZ_ModuleBehaviour b = OZ_PdaModules.ForClass(cls);
            if (!b)
                continue;

            float period = b.TickSeconds();
            if (period <= 0)
                continue;   // декларативний модуль, як антена

            m_ModuleAcc[i] = m_ModuleAcc[i] + MODULE_TICK_BASE;
            if (m_ModuleAcc[i] < period)
                continue;

            float dt = m_ModuleAcc[i];
            m_ModuleAcc[i] = 0;
            b.OnTick(this, owner, dt);
        }
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();

        // Клієнт дізнається про вмикання лише звідси -- OnWorkStart до нього
        // не доходить.
        if (m_IsOn)
            StartModuleTicks();
        else
            StopModuleTicks();
    }

    bool  OZ_IsOn()      { return m_IsOn; }
    float OZ_Charge01()  { return m_Charge01; }
    bool  OZ_HasPin()    { return m_Pin != ""; }
    bool  OZ_AutoLock()  { return m_AutoLock; }
    string OZ_SessionUid() { return m_SessionUid; }

    // ---------------------------------------------------------------- замок

    // Автоблокування рахується ЛІНИВО, у момент звернення, а не таймером.
    // Причина проста: вимкнений пристрій у рюкзаку не тікає, і будити заради
    // нього тік -- марна робота на кожному КПК на сервері.
    void OZ_EvaluateLock(float lockAfterMinutes)
    {
        if (!GetGame().IsServer())
            return;

        if (!m_Unlocked || !m_AutoLock || m_Pin == "")
            return;

        if (m_LeftHandsAt == 0)   // досі в руках -- відлік не почався
            return;

        if (lockAfterMinutes <= 0)
            return;

        int elapsedMs = GetGame().GetTime() - m_LeftHandsAt;
        if (elapsedMs >= lockAfterMinutes * 60000)
        {
            m_Unlocked = false;
            SetSynchDirty();
            OZ_Log.Dbg("pda locked itself after being put away");
        }
    }

    bool OZ_IsUnlocked()
    {
        if (m_Pin == "")
            return true;
        return m_Unlocked;
    }

    // Повертає true, якщо код збігся. Лічильник невдач росте тільки тут.
    bool OZ_TryUnlock(string uid, string attempt)
    {
        if (!GetGame().IsServer())
            return false;

        if (m_Pin == "")
            return true;

        if (OZ_IsLockedOut(uid))
            return false;

        if (attempt != m_Pin)
        {
            BumpFails(uid);
            return false;
        }

        m_Unlocked = true;
        SetSynchDirty();
        ResetFails(uid);
        return true;
    }

    void OZ_Lock()
    {
        if (!GetGame().IsServer())
            return;
        m_Unlocked = false;
        SetSynchDirty();
    }

    bool OZ_IsLockedOut(string uid)
    {
        return OZ_FailsFor(uid) >= PIN_MAX_FAILS;
    }

    int OZ_FailsFor(string uid)
    {
        int i = m_FailUid.Find(uid);
        if (i == -1)
            return 0;
        return m_FailCount[i];
    }

    // --------------------------------------------------------------- сесія

    bool OZ_HasSession(string uid, int playerEpoch)
    {
        if (m_SessionUid == "")
            return false;
        if (m_SessionUid != uid)
            return false;
        // Епоха розійшлась -- гравець скинув сесії з іншого пристрою.
        return m_SessionEpoch == playerEpoch;
    }

    // Чи є на пристрої ХОЧ ЯКАСЬ сесія -- незалежно від того, чия й чи жива.
    // Питання окреме від OZ_HasSession навмисно: «сесії немає взагалі» і
    // «сесія є, але чужа» -- різні стани, і плутати їх означало б віддавати
    // чужий пристрій новому власнику з одного дотику.
    bool OZ_HasAnySession()
    {
        return m_SessionUid != "";
    }

    void OZ_OpenSession(string uid, int playerEpoch)
    {
        if (!GetGame().IsServer())
            return;
        m_SessionUid   = uid;
        m_SessionEpoch = playerEpoch;
    }

    // Явне закриття сесії з ЦЬОГО пристрою. Знімок навмисно НЕ чистимо:
    // вийти з пристрою й стерти з нього все -- різні дії, і другу гравець
    // має робити свідомо.
    void OZ_CloseSession()
    {
        if (!GetGame().IsServer())
            return;
        m_SessionUid   = "";
        m_SessionEpoch = 0;
    }

    // -------------------------------------------------------------- знімок

    // Онлайн -- це коли епоха пристрою збігається з епохою гравця. Розійшлись
    // -- пристрій живий, читається, але більше не оновлюється.
    bool OZ_IsOnline(int playerEpoch)
    {
        if (m_SessionUid == "")
            return false;
        return m_SessionEpoch == playerEpoch;
    }

    string OZ_Snapshot()   { return m_Snapshot; }
    string OZ_SnapshotAt() { return m_SnapshotAt; }

    // Пише лише сервер і лише поки пристрій онлайн.
    void OZ_RefreshSnapshot(int playerEpoch, string json)
    {
        if (!GetGame().IsServer())
            return;
        if (!OZ_IsOnline(playerEpoch))
            return;

        m_Snapshot   = json;
        m_SnapshotAt = OZ_Time.NowUtc();
    }

    // Стерти вміст свідомо -- окрема дія, доступна тому, у кого пристрій
    // відімкнений у руках.
    void OZ_WipeSnapshot()
    {
        if (!GetGame().IsServer())
            return;
        if (!OZ_IsUnlocked())
            return;

        m_Snapshot   = "";
        m_SnapshotAt = "";
    }

    // Щоб змінити пін, його треба ЗНАТИ. Сесія тут ні до чого: пристрій не
    // питає, хто ти, він питає старий код. Немає коду -- задати новий може
    // будь-хто, у кого пристрій у руках.
    //
    // Невдала спроба рахується так само, як невдале відмикання: інакше
    // «зміна піна» стала б обхідним шляхом для підбору.
    bool OZ_SetPin(string uid, string oldPin, string newPin)
    {
        if (!GetGame().IsServer())
            return false;

        if (m_Pin != "")
        {
            if (OZ_IsLockedOut(uid))
                return false;

            if (oldPin != m_Pin)
            {
                BumpFails(uid);
                return false;
            }
        }

        m_Pin = newPin;
        m_Unlocked = true;   // не замикаємо того, хто щойно задав код
        SetSynchDirty();
        ResetFails(uid);
        return true;
    }

    // Автоблокування вимикає будь-хто, у кого пристрій відімкнений у руках.
    // Сервер може заборонити його вимикати зовсім -- це рішення адміна, а не
    // гравця.
    bool OZ_SetAutoLock(bool on, bool serverForces)
    {
        if (!GetGame().IsServer())
            return false;
        if (!OZ_IsUnlocked())
            return false;

        if (serverForces && !on)
            return false;

        m_AutoLock = on;
        return true;
    }

    private void BumpFails(string uid)
    {
        int i = m_FailUid.Find(uid);
        if (i == -1)
        {
            m_FailUid.Insert(uid);
            m_FailCount.Insert(1);
            return;
        }
        m_FailCount[i] = m_FailCount[i] + 1;
    }

    private void ResetFails(string uid)
    {
        int i = m_FailUid.Find(uid);
        if (i != -1)
            m_FailCount[i] = 0;
    }

    // Відлік автоблокування починається, коли пристрій пішов З РУК.
    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        if (!GetGame().IsServer())
            return;

        if (newLoc.GetType() == InventoryLocationType.HANDS)
            m_LeftHandsAt = 0;
        else if (oldLoc.GetType() == InventoryLocationType.HANDS)
            m_LeftHandsAt = GetGame().GetTime();
    }

    // ------------------------------------------------------------- залізо

    EntityAI OZ_Attached(string slotName)
    {
        int idx = InventorySlots.GetSlotIdFromString(slotName);
        if (idx == -1)
            return null;
        return GetInventory().FindAttachment(idx);
    }

    // Класнейм модуля у відсіку i, або порожній рядок.
    string OZ_ModuleClass(int i)
    {
        EntityAI m = OZ_Attached(OZ_PdaConst.ModuleSlot(i));
        if (!m)
            return "";
        return m.GetType();
    }

    // Чи вставлено модуль такого виду. Питаємо ВИД, а не класнейм: КПК не
    // мусить знати, чия саме антена вставлена, щоб зрозуміти, що зв'язок є.
    bool OZ_HasModuleKind(string kind)
    {
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string cls = OZ_ModuleClass(i);
            if (cls == "")
                continue;

            OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
            if (spec && spec.Kind == kind)
                return true;
        }
        return false;
    }

    // Сумарний множник витрати живлення від усіх вставлених модулів.
    float OZ_PowerFactor()
    {
        float f = 1.0;
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string cls = OZ_ModuleClass(i);
            if (cls == "")
                continue;

            OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
            if (spec)
                f *= spec.PowerFactor;
        }
        return f;
    }

    // Ховає відсіки понад те, що дозволяє профіль. Слоти не додаються в
    // рантаймі, тому в конфізі їх максимум, а профіль ріже видиме.
    override bool CanDisplayAttachmentSlot(int slot_id)
    {
        if (!super.CanDisplayAttachmentSlot(slot_id))
            return false;

        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(GetType());
        if (!prof)
            return true;

        for (int i = prof.ModuleSlots; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            if (slot_id == InventorySlots.GetSlotIdFromString(OZ_PdaConst.ModuleSlot(i)))
                return false;
        }
        return true;
    }

    string OZ_CarrierClass()
    {
        EntityAI c = OZ_Attached(OZ_PdaConst.SLOT_CARRIER);
        if (!c)
            return "";
        return c.GetType();
    }

    // ----------------------------------------------------------- живлення

    // Вмикання/вимикання на прохання інтерфейсу.
    //
    // Той самий важіль, що й ванільні ActionTurnOnWhileInHands /
    // ActionTurnOffWhileInHands, які вже висять у SetActions: пристрій живе
    // без нашого інтерфейсу зовсім -- підняв, вставив батарею, увімкнув.
    // Кнопка на сторінці «Пристрій» смикає той самий CompEM, тільки через
    // сервер, а не повз нього.
    //
    // Відповідає рядком-ПРИЧИНОЮ, а не мовчазним false: «немає батареї» й
    // «пристрій не вміє вмикатись» -- різні речі, і гравцеві треба сказати,
    // котра з них.
    string OZ_SetPower(bool on)
    {
        if (!HasEnergyManager())
            return "STR_OZ_ERR_NO_POWER";

        ComponentEnergyManager em = GetCompEM();

        if (on)
        {
            if (!em.GetEnergySource())
                return "STR_OZ_ERR_NO_BATTERY";
            if (!em.CanSwitchOn())
                return "STR_OZ_ERR_NO_POWER";
            em.SwitchOn();
        }
        else
        {
            em.SwitchOff();
        }

        PushState();
        return "";
    }

    override void OnWork(float consumed_energy)
    {
        super.OnWork(consumed_energy);
        if (GetGame().IsServer())
            PushState();
    }

    override void OnWorkStart()
    {
        super.OnWorkStart();
        if (GetGame().IsServer())
        {
            PushState();
            StartModuleTicks();
        }
    }

    override void OnWorkStop()
    {
        super.OnWorkStop();
        if (GetGame().IsServer())
        {
            PushState();
            StopModuleTicks();
        }
    }

    // Батарея у своєму гнізді. Питати про неї треба САМЕ так, а не через
    // GetEnergySource(): джерело з'являється лише коли пристрій увімкнений,
    // тож вимкнений КПК із повною батареєю відповідав би «батареї немає».
    EntityAI OZ_Battery()
    {
        return OZ_Attached(OZ_PdaConst.SLOT_BATTERY);
    }

    bool OZ_HasBattery()
    {
        return OZ_Battery() != null;
    }

    // Заряд беремо з БАТАРЕЇ, а не з себе: energyStorageMax=0, свого запасу
    // пристрій не має й не повинен.
    private void PushState()
    {
        m_IsOn = false;
        m_Charge01 = 0;

        if (HasEnergyManager())
            m_IsOn = GetCompEM().IsWorking();

        EntityAI batt = OZ_Battery();
        if (batt && batt.HasEnergyManager())
        {
            float maxE = batt.GetCompEM().GetEnergyMax();
            if (maxE > 0)
                m_Charge01 = batt.GetCompEM().GetEnergy() / maxE;
        }

        SetSynchDirty();
    }

    // ----------------------------------------------------- персистентність
    //
    // ДОПИСУВАТИ тільки в кінець і читати за GetVersion(). Записи CF
    // позиційні: вставка поля в середину зсуває потік і з'їдає дані всіх, хто
    // пише після нас.
    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return;

        ctx.Write(m_Pin);
        ctx.Write(m_AutoLock);
        ctx.Write(m_SessionUid);
        ctx.Write(m_SessionEpoch);
        ctx.Write(m_Snapshot);
        ctx.Write(m_SnapshotAt);
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage))
            return false;

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return true;

        if (!ctx.Read(m_Pin))
            return false;
        if (!ctx.Read(m_AutoLock))
            return false;
        if (!ctx.Read(m_SessionUid))
            return false;
        if (!ctx.Read(m_SessionEpoch))
            return false;
        if (!ctx.Read(m_Snapshot))
            return false;
        if (!ctx.Read(m_SnapshotAt))
            return false;

        // Замок після рестарту закритий: стан «відімкнено» навмисно не
        // зберігається. Пристрій, що пролежав у схроні через рестарт, має
        // питати код.
        m_Unlocked = false;
        m_LeftHandsAt = 0;

        return true;
    }

    override void SetActions()
    {
        super.SetActions();

        AddAction(ActionTurnOnWhileInHands);
        AddAction(ActionTurnOffWhileInHands);
        AddAction(OZ_ActionOpenPda);
    }
}
