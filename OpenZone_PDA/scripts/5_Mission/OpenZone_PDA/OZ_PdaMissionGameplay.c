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

        OZ_PdaMenuGate.Bind(new OZ_PdaMenuOpener());
        OZ_PdaInput.Init();
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);
        OZ_PdaInput.Poll();
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
#endif

        return menu;
    }
}
