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

    override void OnExecuteClient(ActionData action_data)
    {
        OZ_PdaMenuGate.Open();
    }
}
