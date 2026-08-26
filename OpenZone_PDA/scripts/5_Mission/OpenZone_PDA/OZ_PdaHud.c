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

        // Гасне разом із ванільним інтерфейсом, а не світиться поверх нього.
        // Досі Show() не мав ЖОДНОГО вызивающего, тож смужка малювалась і
        // поверх відкритого КПК, і в інвентарі, і над непритомним гравцем.
        if (!Visible())
        {
            if (s_Root)
                s_Root.Show(false);
            return;
        }

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
    // ВЛАСНИЙ буфер, не спільний з OZ_PdaLookup: у розміщеній грі обидва
    // живуть в одній скриптовій машині, і один масив на двох ходоків -- це
    // тиха поломка того дня, коли один із них покличе другого.
    private static ref array<EntityAI> s_Walk;

    private static EntityAI Device()
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p)
            return null;

        OZ_PDA_Base inHands = OZ_PDA_Base.Cast(p.GetItemInHands());
        if (inHands)
            return inHands;

        GameInventory inv = p.GetInventory();
        if (!inv)
            return null;

        // Смужка тікає двічі на секунду в КОЖНОГО гравця, і кожен такт
        // алокував новий масив -- на порожніх руках це дві алокації й два
        // повні обходи інвентаря за секунду, вічно, заради відсотка заряду.
        // Буфер прибирає алокації; обхід лишається, бо кеш пристрою -- це
        // окрема робота зі своїми правилами вивітрювання.
        if (!s_Walk)
            s_Walk = new array<EntityAI>();

        inv.EnumerateInventory(InventoryTraversalType.PREORDER, s_Walk);
        for (int i = 0; i < s_Walk.Count(); i++)
        {
            OZ_PDA_Base pda = OZ_PDA_Base.Cast(s_Walk[i]);
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

    // Чи має смужка бути видимою ЗАРАЗ. Одна функція на всі причини гасіння
    // -- інакше кожне нове місце гасило б по-своєму, і одне з них рано чи
    // пізно розійшлося б з рештою.
    private static bool Visible()
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p || !p.IsAlive())
            return false;

        UIManager ui = GetGame().GetUIManager();
        if (!ui)
            return false;

        // Наш власний КПК відкритий -- екран каже все те саме, і краще.
        if (ui.FindMenu(OZ_PdaConst.MENU_PDA))
            return false;

        // Будь-яке інше меню поверх світу: пауза, налаштування, чуже вікно.
        if (ui.GetMenu())
            return false;

        // Прапорці ванільного інтерфейсу. IngameHud.Cast -- саме так до них
        // ходить і сама ваниль (gesturesmenu.c:228, continuousactionprogress.c:60).
        IngameHud hud = IngameHud.Cast(GetGame().GetMission().GetHud());
        if (hud)
        {
            IngameHudVisibility vis = hud.GetHudVisibility();
            if (vis)
            {
                // По одній перевірці на рядок, і це не стиль. Умова `if`
                // у Enforce мусить уміщатися в ОДИН рядок: перенесення дає
                // "Expected ')', not a '||'" незалежно від того, стоїть
                // оператор наприкінці рядка чи на початку наступного.
                // Перевірено на стенді 2026-08-26, обидва варіанти.
                if (vis.IsContextFlagActive(EHudContextFlags.HUD_HIDE))
                    return false;
                if (vis.IsContextFlagActive(EHudContextFlags.HUD_DISABLE))
                    return false;
                if (vis.IsContextFlagActive(EHudContextFlags.INVENTORY_OPEN))
                    return false;
                if (vis.IsContextFlagActive(EHudContextFlags.MENU_OPEN))
                    return false;
                if (vis.IsContextFlagActive(EHudContextFlags.UNCONSCIOUS))
                    return false;
            }
        }

        return true;
    }

    // Знімає корінь із робочої області.
    //
    // Ensure() створює віджет ПРЯМО на робочій області, а не всередині чийогось
    // дерева, тож ніхто, крім нас, його не прибере. Досі мод не мав
    // OnMissionFinish узагалі -- отже кожен перезапуск місії лишав по одній
    // смужці назавжди.
    static void Teardown()
    {
        if (s_Root)
        {
            s_Root.Unlink();
            s_Root = null;
        }

        s_Power = null;
        s_Acc   = 0;
        s_Walk  = null;
    }
}
