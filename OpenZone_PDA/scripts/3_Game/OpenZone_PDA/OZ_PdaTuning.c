// Tuning.json -- ігрові ходові ручки КПК одним файлом.
//
// Все, що адмін хоче підкрутити МІЖ рестартами, живе тут, а не в
// константах: межі текстів, лічильники спроб, радіуси, секунди. Константи
// в OZ_PdaConst лишились ЗАПАСНИМИ значеннями -- клієнт і ранній код
// беруть їх, поки конфіг ще не прочитано.
//
// Частина значень потрібна КЛІЄНТОВІ (тривалість тоста, радіус автопроходу
// маршруту) -- їх він отримує не звідси, а в конверті beacon-пуша:
// сервер раз на тік докладає два числа, і окремий канал не потрібен.

class OZ_PdaTuning : OZ_ConfigBase
{
    // --- PIN ---
    // Скільки невдалих спроб підряд замикає пристрій для цієї особи.
    int PinMaxFails = 5;
    // На скільки секунд. 0 -- до рестарту сервера (стара поведінка).
    int PinLockoutSeconds = 300;

    // --- chat ---
    int ChatMsgMaxBytes   = 1000;
    int ChatTitleMaxBytes = 32;
    int ChatDescMaxBytes  = 96;
    // Скільки останніх рядків віддає міст при відкритті розмови...
    int ChatHistoryOpen = 20;
    // ...і скільки довантажує LOAD OLDER за один крок.
    int ChatHistoryPage = 20;
    // Скільки живе запрошення до групи (ТЗ-4 R-D3.3): своє поле, своє
    // число -- фракційне (Faction.InviteTtlSeconds, 120 с) для групи
    // замале: пропозиція має пережити ніч. Міст прибирає протухлі сам.
    int GroupInviteTtlSeconds = 86400;
    // Стеля складу групи. 0 -- без межі. Перевіряє МІСТ при запрошенні.
    int ChatGroupMax = 16;

    // --- нотатки ---
    // Запасна стеля кількості, коли профіль пристрою не каже своєї.
    int NoteTitleMaxBytes = 64;
    // Не більше 1000: JsonFileLoader ріже рядки на 1023 байтах, і міст
    // кліпає на тій самій межі. Validate не дасть поставити більше.
    int NoteBodyMaxBytes = 1000;

    // --- мітки карти ---
    int MarkerNameMaxBytes = 32;
    int MarkerDescMaxBytes = 160;

    // --- контакти ---
    // З якої відстані інший гравець вважається «поруч» для обміну.
    int FriendReachMeters = 12;
    // Скільки живе пропозиція обміну контактами.
    int SwapOfferTtlSeconds = 60;

    // --- HUD (їде клієнтові в beacon-пуші) ---
    int ToastSeconds = 8;
    int RouteAdvanceMeters = 30;
    // Як часто сервер розсилає маячки власникам працюючих антен.
    int BeaconPushSeconds = 5;

    private static ref OZ_PdaTuning s_Inst;

    static OZ_PdaTuning Get()
    {
        return s_Inst;
    }

    override int LatestVersion()
    {
        return 2;
    }

    override void LoadDefaults()
    {
        Version = LatestVersion();

        PinMaxFails       = 5;
        PinLockoutSeconds = 300;

        ChatMsgMaxBytes   = 1000;
        ChatTitleMaxBytes = 32;
        ChatDescMaxBytes  = 96;
        ChatHistoryOpen   = 20;
        ChatHistoryPage   = 20;
        ChatGroupMax      = 16;
        GroupInviteTtlSeconds = 86400;

        NoteTitleMaxBytes = 64;
        NoteBodyMaxBytes  = 1000;

        MarkerNameMaxBytes = 32;
        MarkerDescMaxBytes = 160;

        FriendReachMeters   = 12;
        SwapOfferTtlSeconds = 60;

        ToastSeconds       = 8;
        RouteAdvanceMeters = 30;
        BeaconPushSeconds  = 5;
    }

    override bool Migrate(int from)
    {
        // v2: блокування пiна стало в СЕКУНДАХ (PinLockoutSeconds замiсть
        // хвилин). Старе поле не переноситься: схема прожила лiченi години,
        // и значення за замовчуванням тi самi 5 хвилин.
        Version = LatestVersion();
        return true;
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        warnings += ClampMin("PinMaxFails", PinMaxFails, 1);
        PinMaxFails = Math.Max(PinMaxFails, 1);
        warnings += ClampMin("PinLockoutSeconds", PinLockoutSeconds, 0);
        PinLockoutSeconds = Math.Max(PinLockoutSeconds, 0);

        // Вище 1000 байтів не можна НІДЕ, де текст їздить через
        // JsonFileLoader чи міст: різ на 1023 псує UTF-8 посеред знака.
        if (NoteBodyMaxBytes > 1000)
        {
            OZ_Log.Warn("Tuning: NoteBodyMaxBytes over 1000 breaks storage strings, clamped");
            NoteBodyMaxBytes = 1000;
            warnings++;
        }
        if (ChatMsgMaxBytes > 1000)
        {
            OZ_Log.Warn("Tuning: ChatMsgMaxBytes over 1000 breaks the bridge clip, clamped");
            ChatMsgMaxBytes = 1000;
            warnings++;
        }

        warnings += ClampMin("ChatMsgMaxBytes", ChatMsgMaxBytes, 16);
        ChatMsgMaxBytes = Math.Max(ChatMsgMaxBytes, 16);
        warnings += ClampMin("ChatHistoryOpen", ChatHistoryOpen, 1);
        ChatHistoryOpen = Math.Max(ChatHistoryOpen, 1);
        warnings += ClampMin("ChatHistoryPage", ChatHistoryPage, 1);
        ChatHistoryPage = Math.Max(ChatHistoryPage, 1);
        warnings += ClampMin("ChatGroupMax", ChatGroupMax, 0);
        warnings += ClampMin("GroupInviteTtlSeconds", GroupInviteTtlSeconds, 60);
        GroupInviteTtlSeconds = Math.Max(GroupInviteTtlSeconds, 60);
        ChatGroupMax = Math.Max(ChatGroupMax, 0);


        warnings += ClampMin("FriendReachMeters", FriendReachMeters, 1);
        FriendReachMeters = Math.Max(FriendReachMeters, 1);
        warnings += ClampMin("SwapOfferTtlSeconds", SwapOfferTtlSeconds, 5);
        SwapOfferTtlSeconds = Math.Max(SwapOfferTtlSeconds, 5);

        warnings += ClampMin("ToastSeconds", ToastSeconds, 2);
        ToastSeconds = Math.Max(ToastSeconds, 2);
        warnings += ClampMin("RouteAdvanceMeters", RouteAdvanceMeters, 5);
        RouteAdvanceMeters = Math.Max(RouteAdvanceMeters, 5);
        warnings += ClampMin("BeaconPushSeconds", BeaconPushSeconds, 2);
        BeaconPushSeconds = Math.Max(BeaconPushSeconds, 2);
    }

    // Попередити про замале значення. Сам кламп робить викликач:
    // Enforce не передає int за посиланням у довільні помічники.
    private int ClampMin(string name, int val, int floor)
    {
        if (val >= floor)
            return 0;
        OZ_Log.Warn("Tuning: " + name + " under " + floor.ToString() + ", clamped");
        return 1;
    }

    static void ServerLoad()
    {
        s_Inst = new OZ_PdaTuning();
        OZ_ConfigLoader<OZ_PdaTuning>.Load(OZ_Const.PROFILE_DIR + "\\OZ_PDA_Tuning.json", "Tuning", s_Inst);
    }
}

// Читачі з запасними значеннями: сервер бере конфіг, клієнт і ранній
// код -- старі константи. Одне місце, одна форма звернення.
class OZ_PdaTune
{
    static int PinMaxFails()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.PinMaxFails;
        return 5;
    }

    static int PinLockoutMs()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.PinLockoutSeconds * 1000;
        return 300000;
    }

    static int ChatMsgMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.ChatMsgMaxBytes;
        return OZ_PdaConst.CHAT_MSG_MAX;
    }

    static int ChatTitleMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.ChatTitleMaxBytes;
        return OZ_PdaConst.CHAT_TITLE_MAX;
    }

    static int ChatDescMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.ChatDescMaxBytes;
        return OZ_PdaConst.CHAT_DESC_MAX;
    }

    static int ChatHistoryOpen()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.ChatHistoryOpen;
        return OZ_PdaConst.CHAT_HISTORY_OPEN;
    }

    static int ChatHistoryPage()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.ChatHistoryPage;
        return OZ_PdaConst.CHAT_HISTORY_PAGE;
    }

    static int GroupInviteTtlS()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.GroupInviteTtlSeconds;
        return 86400;
    }

    static int ChatGroupMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.ChatGroupMax;
        return OZ_PdaConst.CHAT_GROUP_MAX;
    }


    static int NoteTitleMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.NoteTitleMaxBytes;
        return OZ_PdaConst.NOTE_TITLE_MAX;
    }

    static int NoteBodyMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.NoteBodyMaxBytes;
        return OZ_PdaConst.NOTE_BODY_MAX;
    }

    static int MarkerNameMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.MarkerNameMaxBytes;
        return OZ_PdaConst.MARKER_NAME_MAX;
    }

    static int MarkerDescMax()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.MarkerDescMaxBytes;
        return OZ_PdaConst.MARKER_DESC_MAX;
    }

    static int FriendReachM()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.FriendReachMeters;
        return OZ_PdaConst.FRIEND_REACH_M;
    }

    static int SwapOfferTtlMs()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.SwapOfferTtlSeconds * 1000;
        return 60000;
    }

    static int ToastSeconds()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.ToastSeconds;
        return 8;
    }

    static int RouteAdvanceM()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.RouteAdvanceMeters;
        return 30;
    }

    static float BeaconPushSeconds()
    {
        OZ_PdaTuning t = OZ_PdaTuning.Get();
        if (t)
            return t.BeaconPushSeconds;
        return 5;
    }
}
