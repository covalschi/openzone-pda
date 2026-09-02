// Клієнтська точка входу КПК: реєстрація меню, шлюз відкриття, опитування
// клавіші.
//
// На виділеному сервері MissionGameplay не створюється взагалі (там
// MissionServer), тож цей код туди просто не потрапляє.

class OZ_PdaMenuOpener : OZ_PdaMenuGate
{
    override void DoOpen()
    {
        UIManager ui = GetGame().GetUIManager();

        // Реєстру id меню в рушії немає, тож зіткнення з ЧУЖИМ модом можливе
        // й непереборне. Єдиний захист -- не відкривати те, що вже відкрито.
        if (ui.FindMenu(OZ_PdaConst.MENU_PDA))
            return;

        // EnterScriptedMenu, а НЕ ShowScriptedMenu: друге позначає меню як
        // «створене приховано», і LockControls мовчки не спрацьовує -- ні
        // курсора, ні блокування керування.
        ui.EnterScriptedMenu(OZ_PdaConst.MENU_PDA, null);
    }

    override void DoClose()
    {
        GetGame().GetUIManager().CloseMenu(OZ_PdaConst.MENU_PDA);
    }
}

modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();

        // Ядро розносить команди «покажи це»; нас цікавить одна.
        OZ_Show.OnShow.Insert(OZ_PdaShow);

        // ВІДПОВІДІ, ЯКІ ТРЕБА ПОКАЗАТИ БЕЗ КПК.
        //
        // Обмін контактами відбувається в світі, з ЗАКРИТИМ приладом: сторінка
        // контактів у цей момент не існує й почути нічого не може. Двоє
        // стояли, тикали приладами один в одного й не бачили ані «запропоновано»,
        // ані «обмінялись» -- взагалі нічого.
        //
        // Тут -- місія, вона жива завжди. Показуємо тим самим сповіщенням,
        // яким гра говорить про все інше.
        OZ_RoleNotice.OnAnswer.Insert(OZ_PdaNotice);

        OZ_PdaMenuGate.Bind(new OZ_PdaMenuOpener());
        OZ_PdaInput.Init();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        OZ_PdaInput.Poll();
        OZ_PdaHud.Update(timeslice);
    }

    // Мод не мав цього хука взагалі, а смужка живе прямо на робочій області
    // -- отже кожен перезапуск місії лишав по одній назавжди. Тепер знімаємо
    // за собою.
    override void OnMissionFinish()
    {
        // Дзеркало підписок з OnMissionStart: інвокери статичні й переживуть
        // місію, а місія -- ні. Слухач, що пережив свою місію, отримує
        // наступну подію вже з мертвими руками.
        OZ_Show.OnShow.Remove(OZ_PdaShow);
        OZ_RoleNotice.OnAnswer.Remove(OZ_PdaNotice);

        OZ_PdaHud.Teardown();
        super.OnMissionFinish();
    }

    // Показуємо лише те, що стосується світу, а не меню: коли КПК відкритий,
    // сторінка контактів скаже те саме своєю підказкою, і два повідомлення про
    // одне гірші за одне.
    void OZ_PdaNotice(string op, bool ok, string why)
    {
        if (op != "swap")
            return;

        NotificationSystem.AddNotificationExtended(4, "#STR_OZ_PDA_NAME", OZ_RoleNotice.Text(), "");
    }

    void OZ_PdaShow(string what)
    {
        if (what == "pda")
            OZ_PdaMenuGate.Open();

        // Той самий екран без предмета за ним (D132): місія запам'ятовує це у
        // шлюзі, і меню не закриється через порожні руки.
        if (what == "pda_virtual")
            OZ_PdaMenuGate.OpenVirtual();
    }

    override UIScriptedMenu CreateScriptedMenu(int id)
    {
        // super ПЕРШИЙ і вихід одразу, якщо він щось віддав: саме це тримає
        // сумісність з іншими модами, що чіпали той самий клас.
        UIScriptedMenu menu = super.CreateScriptedMenu(id);
        if (menu)
            return menu;

#ifndef NO_GUI
        if (id == OZ_PdaConst.MENU_PDA)
        {
            menu = new OZ_PdaMenu();
            menu.SetID(id);
        }

        if (id == OZ_PdaConst.MENU_PDA_HUD)
        {
            menu = new OZ_HudEditMenu();
            menu.SetID(id);
        }
#endif

        return menu;
    }
}
