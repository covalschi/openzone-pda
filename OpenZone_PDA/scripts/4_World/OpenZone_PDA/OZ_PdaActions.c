// Дія «відкрити КПК».
//
// Форма взята з ванільного ActionTurnOnWhileInHands: та сама база, ті самі
// компоненти умов. Відкриття -- справа КЛІЄНТА (меню існує тільки в нього),
// тому вся робота в OnExecuteClient, а серверу тут робити нічого.

class OZ_ActionOpenPda : ActionSingleUseBase
{
    void OZ_ActionOpenPda()
    {
        m_Text = "#STR_OZ_ACTION_OPEN";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined();
        m_ConditionTarget = new CCTNone();
    }

    override bool HasTarget()
    {
        return false;
    }

    override bool HasProneException()
    {
        return true;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        OZ_PDA_Base pda = OZ_PDA_Base.Cast(item);
        if (!pda)
            return false;

        // Пристрій без живлення не відкривається. Якщо конфіг предмета
        // взагалі не має EnergyManager -- відкриваємо: значить живлення для
        // нього не передбачене й вимагати його нема сенсу.
        if (!pda.HasEnergyManager())
            return true;

        return pda.GetCompEM().IsWorking();
    }

    // OnStartClient, а НЕ OnExecuteClient.
    //
    // Друге кличеться з події анімації (animatedactionbase.c:199), тобто лише
    // якщо жест дограв до кінця. Для відкриття екрана це неправильний гачок
    // двічі: гравець чекає зайву мить, а перервана анімація -- зачепився,
    // рушив, дістав по голові -- лишає його з приладом у руці й без екрана,
    // хоча дія «спрацювала».
    //
    // OnStartClient спрацьовує на Start() (actionbase.c:758), одразу й
    // безумовно. Це саме те, чого хоче дія, яка нічого не змінює у світі, а
    // лише показує вікно.
    // ВІДКРИВАЄ СЕРВЕР, і це не примха.
    //
    // Клієнтська половина дії тут не спрацьовує: ActionManager кличе Start()
    // на клієнті лише після підтвердження сервера й лише якщо пройде повторна
    // перевірка умов (actionmanagerclient.c:70-85). Виміряно на стенді --
    // серверна половина відпрацьовувала щоразу (список дій скидався), меню не
    // відкривалось жодного. Ані OnStartClient, ані OnExecuteClient.
    //
    // Замість того щоб гадати про чужий скінченний автомат, беремо канал,
    // який доведено працює весь час: сервер зробив -- сервер і сказав.
    override void OnExecuteServer(ActionData action_data)
    {
        if (!action_data.m_Player)
            return;

        OZ_Rpc.Show(action_data.m_Player.GetIdentity(), "pda");
    }
}

// Дія «обмінятись контактами».
//
// Обмін відбувається В СВІТІ, а не в меню: підійшов, дістав прилад, навів на
// людину. Рішення власника, і воно прибирає з КПК цілий шар інтерфейсу --
// у списку більше немає ні незнайомців поруч, ні вхідних запитів, ні кнопок
// «додати/прийняти/відхилити». Список -- це просто твої контакти.
//
// ВЗАЄМНІСТЬ через ту саму дію, а не через окреме «прийняти». Перший тик
// лишає пропозицію, другий -- зустрічний -- замикає обмін. Двоє справді
// тикають приладами один в одного, і жодна половина не відбувається здалеку.
//
// Уся робота на СЕРВЕРІ: контакти -- це файли акаунтів, і клієнту тут
// нічого вирішувати.

class OZ_ActionExchangeContacts : ActionSingleUseBase
{
    void OZ_ActionExchangeContacts()
    {
        m_Text = "#STR_OZ_ACTION_EXCHANGE";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINonRuined();

        // Ціль -- ЛЮДИНА в межах звичайної дистанції дії. Та сама умова, якою
        // ваніль користується для всього, що роблять з іншим гравцем.
        m_ConditionTarget = new CCTMan(UAMaxDistances.DEFAULT);
    }

    override bool HasTarget()
    {
        return true;
    }

    override bool HasProneException()
    {
        return true;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        OZ_PDA_Base pda = OZ_PDA_Base.Cast(item);
        if (!pda)
            return false;

        // Вимкнений прилад нічого не робить -- те саме правило, що й у гейта
        // сторінок. Прилад без EnergyManager живлення не потребує взагалі.
        if (pda.HasEnergyManager())
        {
            if (!pda.GetCompEM().IsWorking())
                return false;

            // Замкнений пристрій соціально німий: обмін контактами -- дія
            // СЕСІЇ, а не заліза. Біт синхронний, умова чесна й на клієнті.
            if (pda.OZ_LockedForViewer())
                return false;
        }

        PlayerBase other = PlayerBase.Cast(target.GetObject());
        if (!other)
            return false;
        if (other == player)
            return false;
        if (!other.GetIdentity())
            return false;

        // Мертвому контакти не потрібні.
        if (!other.IsAlive())
            return false;

        // Обмін -- МІЖ ПРИСТРОЯМИ (рішення власника 2026-08-29): і другий
        // учасник мусить ТРИМАТИ свій КПК у руках, увімкнений і
        // відімкнений. Тиснуть руки два термінали, а не дві людини --
        // записується акаунт, до якого прив'язаний пристрій.
        OZ_PDA_Base otherPda = OZ_PDA_Base.Cast(other.GetItemInHands());
        if (!otherPda)
            return false;
        if (otherPda.HasEnergyManager())
        {
            if (!otherPda.GetCompEM().IsWorking())
                return false;
            if (otherPda.OZ_LockedForViewer())
                return false;
        }

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        if (!action_data.m_Player)
            return;

        PlayerBase other = PlayerBase.Cast(action_data.m_Target.GetObject());
        if (!other)
            return;

        // Акаунти беруться З ПРИСТРОЇВ, не з тих, хто їх тримає: контакт
        // прив'язаний до сесії КПК. Нічийний термінал ні за кого не
        // говорить, капсула нічого не міняє -- обидва чесно відмовляють.
        OZ_PDA_Base myPda = OZ_PDA_Base.Cast(action_data.m_Player.GetItemInHands());
        OZ_PDA_Base otherPda = OZ_PDA_Base.Cast(other.GetItemInHands());
        if (!myPda || !otherPda)
            return;

        PlayerIdentity fromId = action_data.m_Player.GetIdentity();

        if (myPda.OZ_SessionUid() == "" || otherPda.OZ_SessionUid() == "")
        {
            OZ_PdaContactSwap.Say(fromId, "STR_OZ_ERR_NOT_INIT");
            return;
        }

        if (OZ_PdaCapsule.IsFrozen(myPda) || OZ_PdaCapsule.IsFrozen(otherPda))
        {
            OZ_PdaContactSwap.Say(fromId, "STR_OZ_ERR_FROZEN");
            return;
        }

        OZ_PdaContactSwap.Offer(fromId, other.GetIdentity(), myPda.OZ_SessionUid(), otherPda.OZ_SessionUid());
    }
}
