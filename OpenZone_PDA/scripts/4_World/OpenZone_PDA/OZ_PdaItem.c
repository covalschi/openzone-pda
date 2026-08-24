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
//   СЕСІЯ належить ГРАВЦЕВІ. Пристрій пам'ятає, чия сесія на ньому відкрита,
//   і тільки власник сесії може змінити пін чи вимкнути автоблокування. Той,
//   кому КПК просто передали, читає й пише -- але не переналаштовує.
//
// Сесія живе, поки епоха пристрою збігається з епохою гравця в його файлі.
// «Скинути інші сесії» -- це епоха += 1, після чого всі решта пристроїв
// мовчки розлогінюються самі. Ніякого обліку чужих предметів, переживає
// рестарт, коштує одну операцію.
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

    string OZ_AntennaClass()
    {
        EntityAI a = OZ_Attached(OZ_PdaConst.SLOT_ANTENNA);
        if (!a)
            return "";
        return a.GetType();
    }

    string OZ_CarrierClass()
    {
        EntityAI c = OZ_Attached(OZ_PdaConst.SLOT_CARRIER);
        if (!c)
            return "";
        return c.GetType();
    }

    // ----------------------------------------------------------- живлення

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
            PushState();
    }

    override void OnWorkStop()
    {
        super.OnWorkStop();
        if (GetGame().IsServer())
            PushState();
    }

    // Заряд беремо з ДЖЕРЕЛА, а не з себе: energyStorageMax=0, свого запасу
    // пристрій не має й не повинен.
    private void PushState()
    {
        m_IsOn = false;
        m_Charge01 = 0;

        if (HasEnergyManager())
        {
            ComponentEnergyManager em = GetCompEM();
            m_IsOn = em.IsWorking();

            EntityAI src = em.GetEnergySource();
            if (src && src.HasEnergyManager())
            {
                float maxE = src.GetCompEM().GetEnergyMax();
                if (maxE > 0)
                    m_Charge01 = src.GetCompEM().GetEnergy() / maxE;
            }
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
