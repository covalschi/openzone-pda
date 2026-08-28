// HUD КПК: смужка живлення, тост чату, мінімапа.
//
// НЕ ШЛЕ НІЧОГО. Смужка й мінімапа живуть з того, що вже на клієнті
// (мережеві змінні предмета й позиція гравця), а тост -- з пушів, які
// сервер і так шле кожному одержувачу рядка (OZ_ChatSink). Жодного
// опитування сервера звідси немає й не буде.
//
// АКТИВНІСТЬ: пристрій у руках або в слоті носіння. У рюкзаку він мовчить
// -- рішення власника 2026-08-28, і воно ж зробило пошук дешевим: дві
// вибірки O(1) замість обходу інвентаря.

// Вуха HUD: окремий екземпляр для підписки на ScriptInvoker. Справжня
// пастка була НЕ тут, а в місці озброєння: підписка жила в Ensure(), який
// не біжить, поки гравець стоїть у паузі, -- і пуші летіли повз. Екземпляр
// лишаємо: так робить ваніль, і час життя підписки видно по полю.
class OZ_PdaHudEars
{
    void OnPush(string pageId, string op, bool ok, string json, string error)
    {
        OZ_PdaHud.OnPush(pageId, op, ok, json, error);
    }
}

class OZ_PdaHud
{
    private static Widget s_Root;
    private static Widget s_Strip;
    private static Widget s_Toast;
    private static Widget s_Mini;
    private static TextWidget s_Power;
    private static TextWidget s_ToastWho;
    private static TextWidget s_ToastText;
    private static MapWidget s_MiniMap;
    private static float s_Acc = 0;
    private static bool s_Hooked = false;
    private static ref OZ_PdaHudEars s_Ears;

    // Тост тримається стільки, скільки людина читає один рядок.
    private static float s_ToastUntil = 0;
    private static const float TOAST_HOLD_MS = 6000;

    // Раз на півсекунди. Заряд падає повільно, а от «дістав/сховав» гравець
    // помічає одразу, і чекати цілу секунду тут відчутно.
    private static const float TICK = 0.5;

    // Масштаб мінімапи: близько, «де я і що поруч». Ванільна мапа відкриває
    // себе на ~0.33; чверть від того дає квартал, а не область.
    private static const float MINI_SCALE = 0.08;

    private static const string ICON_SELF = "\\DZ\\gear\\navigation\\data\\map_tshelter_ca.paa";

    static void Update(float timeslice)
    {
#ifdef NO_GUI
        return;
#endif

        // Вуха озброюються ПЕРШИМ же тактом, а не першою видимістю: пуш,
        // що прийшов, поки гравець стояв у паузі, теж мусить подзвонити --
        // хай і тоді, коли HUD знову стане видно.
        if (!s_Hooked)
        {
            s_Hooked = true;
            s_Ears = new OZ_PdaHudEars();
            OZ_ClientState.ResponseWatch().Insert(s_Ears.OnPush);
        }

        s_Acc += timeslice;
        if (s_Acc < TICK)
            return;
        s_Acc = 0;

        // Гасне разом із ванільним інтерфейсом, а не світиться поверх нього.
        if (!Visible())
        {
            if (s_Root)
                s_Root.Show(false);
            return;
        }

        OZ_PDA_Base pda = Device();

        if (!pda)
        {
            if (s_Root)
                s_Root.Show(false);
            return;
        }

        Ensure();
        if (!s_Root)
            return;

        s_Root.Show(true);

        PaintStrip(pda);
        PaintToast(pda);
        PaintMini(pda);
    }

    private static void PaintStrip(OZ_PDA_Base pda)
    {
        if (!s_Strip || !s_Power)
            return;

        s_Strip.Show(true);

        if (!pda.OZ_IsOn())
        {
            s_Power.SetText("#STR_OZ_DEV_OFF");
            return;
        }

        // Відсоток чесний для БУДЬ-ЯКОЇ батареї: частка рахується на сервері
        // від GetEnergyMax() вставленої батареї, модова ємність включно.
        int pct = Math.Round(pda.OZ_Charge01() * 100);
        string t = "#STR_OZ_DEV_POWER";
        t += "  " + pct.ToString() + "%";
        s_Power.SetText(t);
    }

    private static void PaintToast(OZ_PDA_Base pda)
    {
        if (!s_Toast)
            return;

        // Вимкнений пристрій не «пікає»: дзвінок -- робота пристрою.
        bool show = pda.OZ_IsOn() && GetGame().GetTime() < s_ToastUntil;
        s_Toast.Show(show);
    }

    private static void PaintMini(OZ_PDA_Base pda)
    {
        if (!s_Mini || !s_MiniMap)
            return;

        if (!pda.OZ_IsOn())
        {
            s_Mini.Show(false);
            return;
        }

        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p)
        {
            s_Mini.Show(false);
            return;
        }

        s_Mini.Show(true);

        vector at = p.GetPosition();
        s_MiniMap.SetScale(MINI_SCALE);
        s_MiniMap.SetMapPos(at);
        s_MiniMap.ClearUserMarks();
        s_MiniMap.AddUserMark(at, "", ARGB(255, 255, 122, 26), ICON_SELF);
    }

    // Пуш із сервера: рядок чату приїхав, байдуже, відкрите меню чи ні.
    // Свої не дзвонять; відкрите меню показує рядок краще за тост, але
    // Visible() і так гасить увесь HUD, поки будь-яке меню відкрите.
    static void OnPush(string pageId, string op, bool ok, string json, string error)
    {
        if (pageId != OZ_PdaConst.PAGE_CHAT || op != "line" || !ok)
            return;

        OZ_Log.Dbg("hud: chat line push heard");

        OZ_ChatPush p;
        string err;
        if (!JsonFileLoader<OZ_ChatPush>.LoadData(json, p, err) || !p)
            return;

        if (p.Mine)
            return;

        Ensure();
        if (s_ToastWho)
            s_ToastWho.SetText(p.Who);
        if (s_ToastText)
            s_ToastText.SetText(p.Text);
        s_ToastUntil = GetGame().GetTime() + TOAST_HOLD_MS;
    }

    // Той самий пристрій, про який говорить сервер: руки, потім слот
    // носіння. Правило одне на весь мод -- інакше смужка й сервер говорили
    // б про різні речі.
    private static OZ_PDA_Base Device()
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

        int slotId = InventorySlots.GetSlotIdFromString(OZ_PdaConst.SLOT_WEAR);
        if (slotId == -1)
            return null;

        return OZ_PDA_Base.Cast(inv.FindAttachment(slotId));
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

        s_Strip     = s_Root.FindAnyWidget("StripPane");
        s_Power     = TextWidget.Cast(s_Root.FindAnyWidget("HudPower"));
        s_Toast     = s_Root.FindAnyWidget("ToastPane");
        s_ToastWho  = TextWidget.Cast(s_Root.FindAnyWidget("ToastWho"));
        s_ToastText = TextWidget.Cast(s_Root.FindAnyWidget("ToastText"));
        s_Mini      = s_Root.FindAnyWidget("MiniPane");
        s_MiniMap   = MapWidget.Cast(s_Root.FindAnyWidget("MiniMap"));
    }

    // Смужка мусить зникати разом із рештою інтерфейсу: гравець ховає HUD не
    // для того, щоб наш кут лишився світитись.
    static void Show(bool show)
    {
        if (s_Root)
            s_Root.Show(show);
    }

    // Чи має HUD бути видимим ЗАРАЗ. Одна функція на всі причини гасіння
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

        s_Strip     = null;
        s_Power     = null;
        s_Toast     = null;
        s_ToastWho  = null;
        s_ToastText = null;
        s_Mini      = null;
        s_MiniMap   = null;
        s_Ears      = null;
        s_Acc       = 0;
        s_ToastUntil = 0;
    }
}
