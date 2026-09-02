class OZ_PdaConst
{
    static const string PROFILES = "$profile:OpenZone\\OZ_PDA_Profiles.json";

    static const string HARDWARE = "$profile:OpenZone\\OZ_PDA_Hardware.json";

    static const int SCHEMA_PROFILES = 1;

    // Імена слотів. Батарея -- ванільна: рушій сам втикає пристрій у неї.
    // Решта наші, і рушій про них не знає нічого -- що в них влазить і що це
    // дає, вирішує таблиця класнеймів у Hardware.json.
    static const string SLOT_BATTERY = "BatteryD";
    static const string SLOT_CARRIER = "OZ_DataCarrier";
    // Слот носіння на САМОМУ ГРАВЦЕВІ. Пристрій активний лише в руках або
    // тут: у рюкзаку він мовчить -- і за задумом, і тому, що так не треба
    // обходити інвентар на кожен запит.
    static const string SLOT_WEAR    = "OZ_PdaWear";

    // Модульні відсіки. Оголошені ВСІ, бо слоти не додаються в рантаймі;
    // скільки з них видно на конкретній моделі -- вирішує профіль, а зайві
    // ховає CanDisplayAttachmentSlot.
    static const int    MODULE_SLOTS_MAX = 3;
    static const string SLOT_MODULE_1 = "OZ_Module1";
    static const string SLOT_MODULE_2 = "OZ_Module2";
    static const string SLOT_MODULE_3 = "OZ_Module3";

    static string ModuleSlot(int i)
    {
        if (i == 0) return SLOT_MODULE_1;
        if (i == 1) return SLOT_MODULE_2;
        if (i == 2) return SLOT_MODULE_3;
        return "";
    }

    // Види модулів. Рядками, бо ці ж слова стоять у JSON і їх читає адмін.
    static const string MOD_ANTENNA    = "antenna";
    static const string MOD_RADIOMETER = "radiometer";
    static const string MOD_DECRYPTOR  = "decryptor";
    static const string MOD_DOSIMETER  = "dosimeter";

    // Id меню для EnterScriptedMenu.
    //
    // Мусить бути > 46 (останній ванільний у constants.c), але й НЕ завеликим:
    // рушій відкидає завеликий id ще ДО того, як спитати місію -- метод
    // EnterScriptedMenu просто повертає NULL, а Mission.CreateScriptedMenu
    // навіть не викликається. Сусідній мод спіймав це діагностикою: у лозі є
    // виклики з ванільними 11 і 17, а з шестизначним -- жодного.
    //
    // Реєстру id у рушії немає, тож зіткнення з ЧУЖИМ модом можливе й
    // непереборне. Єдиний захист -- перевіряти FindMenu перед відкриттям.
    static const int MENU_PDA = 131;
    // Редактор розкладки HUD -- сусіднє меню з тими самими застереженнями.
    static const int MENU_PDA_HUD = 132;

    // Ім'я інпута з data/inputs.xml. Імена глобальні для ВСІХ завантажених
    // модів, тому префікс обов'язковий.
    static const string INPUT_OPEN = "UAOZPdaOpen";
    static const string INPUT_PTT  = "UAOZPdaPtt";

    // Сторінки, які несе сам КПК.
    //
    // PAGE_QUESTS -- договір, а не вміст: КПК малює журнал, а завдання в нього
    // кладе квестовий мод через OZ_PdaQuests.Bind(). Так журнал лишається
    // один, чий би мод його не наповнював.
    static const string PAGE_DEVICE = "device";
    static const string PAGE_QUESTS = "quests";
    static const string PAGE_CONTACTS = "contacts";
    static const string PAGE_NOTES    = "notes";
    static const string PAGE_MAP      = "map";
    static const string PAGE_NEWS     = "news";
    static const string PAGE_CHAT     = "chat";

    // Межі записок. Не смак, а захист: текст їде в JSON на диску, а згодом у
    // тред Discord, у якого своя межа повідомлення.
    static const int NOTES_MAX      = 50;
    static const int NOTE_TITLE_MAX = 64;
    // 1000, НЕ більше: JsonFileLoader.LoadData ріже значення-рядок до 1023
    // байтів (зміряно зондом 2026-08-28), і стеля вища за це -- обіцянка,
    // якої гра дотримати не може. Міст ріже свої тіла до тих самих 1000.
    static const int NOTE_BODY_MAX  = 1000;

    // Скільки міток -- каже профіль пристрою; тут лише довжина підпису.
    static const int MARKER_NAME_MAX = 32;
    static const int MARKER_DESC_MAX = 160;

    // Наскільки близько треба клікнути, щоб влучити в наявну мітку, у метрах.
    // Не в пікселях: на різних масштабах піксель означає різну відстань, і
    // «влучив» мало б залежати від зуму.
    static const float MARKER_PICK_M = 40;

    // Скільки тримається підказка від СЕРВЕРА, поки чергова перемальовка
    // не має права її затерти. Сторінки, що перепитують себе раз на
    // секунду, інакше губили відмови: між написанням і затиранням
    // проходила частка секунди.
    static const int HINT_HOLD_MS = 4000;

    // На якій відстані можна попросити в друзі. У Зоні знайомляться в очі:
    // 12 метрів -- це «стоїмо поруч», а не «бачу на схилі».
    static const float FRIEND_REACH_M = 12;

    // Межі розмов. Довжина повідомлення -- не смак: текст їде в JSON на диску
    // і згодом у тред Discord, у якого своя межа.
    static const int CHAT_MSG_MAX   = 220;
    static const int CHAT_TITLE_MAX = 32;
    static const int CHAT_DESC_MAX  = 96;
    static const int CHAT_KEEP      = 100;
    static const int CHAT_GROUP_MAX = 16;
}
