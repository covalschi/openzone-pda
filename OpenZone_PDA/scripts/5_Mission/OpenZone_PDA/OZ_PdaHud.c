// Смужка КПК у кутку екрана.
//
// НЕ ШЛЕ НІЧОГО. Усе, що вона показує, уже лежить на клієнті: m_IsOn і
// m_Charge01 -- мережеві змінні самого предмета, і вони приїжджають самі,
// коли міняються. Опитувати ними сервер щосекунди означало б платити
// трафіком за те, що вже прийшло безкоштовно.
//
// Через це смужка працює й тоді, коли меню закрите, і не додає жодного
// пакета на вісімдесят гравців.

class OZ_PdaHud
{
    private static Widget s_Root;
    private static TextWidget s_Power;
    private static float s_Acc = 0;

    // Раз на півсекунди. Заряд падає повільно, а от «дістав/сховав» гравець
    // помічає одразу, і чекати цілу секунду тут відчутно.
    private static const float TICK = 0.5;

    static void Update(float timeslice)
    {
#ifdef NO_GUI
        return;
#endif

        s_Acc += timeslice;
        if (s_Acc < TICK)
            return;
        s_Acc = 0;

        EntityAI dev = Device();

        if (!dev)
        {
            if (s_Root)
                s_Root.Show(false);
            return;
        }

        Ensure();
        if (!s_Root)
            return;

        s_Root.Show(true);

        OZ_PDA_Base pda = OZ_PDA_Base.Cast(dev);
        if (!pda || !s_Power)
            return;

        if (!pda.OZ_IsOn())
        {
            s_Power.SetText("#STR_OZ_DEV_OFF");
            return;
        }

        int pct = Math.Round(pda.OZ_Charge01() * 100);
        string t = "#STR_OZ_DEV_POWER";
        t += "  " + pct.ToString() + "%";
        s_Power.SetText(t);
    }

    // Той самий пристрій, про який говорить меню: спершу руки, потім
    // інвентар. Правило одне на весь мод -- інакше смужка й екран говорили б
    // про різні речі.
    private static EntityAI Device()
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p)
            return null;

        OZ_PDA_Base inHands = OZ_PDA_Base.Cast(p.GetItemInHands());
        if (inHands)
            return inHands;

        array<EntityAI> items = new array<EntityAI>();
        p.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
        for (int i = 0; i < items.Count(); i++)
        {
            OZ_PDA_Base pda = OZ_PDA_Base.Cast(items[i]);
            if (pda)
                return pda;
        }
        return null;
    }

    private static void Ensure()
    {
        if (s_Root)
            return;

        s_Root = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_hud.layout");
        if (!s_Root)
        {
            OZ_Log.Error("pda hud layout failed to load");
            return;
        }

        s_Power = TextWidget.Cast(s_Root.FindAnyWidget("HudPower"));
    }

    // Смужка мусить зникати разом із рештою інтерфейсу: гравець ховає HUD не
    // для того, щоб наш кут лишився світитись.
    static void Show(bool show)
    {
        if (s_Root)
            s_Root.Show(show);
    }
}
