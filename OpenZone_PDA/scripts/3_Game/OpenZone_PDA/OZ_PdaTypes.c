// Форми відповідей сторінок КПК.
//
// Живуть у 3_Game, бо їх серіалізує сервер (4_World) і читає клієнт
// (5_Mission) -- спільним для обох є лише цей шар.

// Один модульний відсік, як його бачить клієнт.
class OZ_BayInfo
{
    int    Index    = 0;
    bool   Visible  = false;   // профіль може ховати зайві відсіки
    string ClassName = "";     // порожньо -- відсік вільний
    string Display  = "";
    string Kind     = "";
}

// Відповідь на device/status.
//
// Тут ЛИШЕ про той пристрій, що в руках. Клієнту не потрібна вся таблиця
// профілів, а надіслати її означало б роздати кожному опис усіх тирів,
// включно з тими, яких він ніколи не побачить.
class OZ_PdaDeviceStatus
{
    // --- пристрій ---
    string ClassName   = "";
    string ProfileId   = "";
    string DisplayName = "";
    ref array<string> Pages;          // з чого будувати стрічку вкладок
    int    ModuleSlots = 0;

    // Мережевий id САМЕ того КПК, про який відповів сервер, і чи він у руках.
    //
    // Клієнт не має права шукати пристрій самотужки: сервер бере руки, а
    // потім інвентар, і якщо клієнт подивиться лише в руки -- він покаже
    // прев'ю порожнечі там, де сервер говорить про цілком конкретний
    // пристрій у рюкзаку. Одна правда, і вона приїжджає звідси.
    bool InHands = false;
    int  NetLow  = 0;
    int  NetHigh = 0;

    // --- живлення ---
    //
    // HasBattery окремо від Charge01: порожнє гніздо й сіла батарея -- різні
    // біди, і кнопка живлення мусить писати різне.
    bool  Powered    = false;
    bool  HasBattery = false;
    float Charge01   = 0;

    // --- залізо ---
    ref array<ref OZ_BayInfo> Bays;
    string CarrierClass = "";
    bool   CarrierWritable = false;
    string CarrierDisplay  = "";
    bool   CarrierWritten = false;
    // Капсула часу: вміст знімка і його дата -- лише коли пристрій офлайн.
    // Ім'я власника сесії -- завжди, коли сесія є (пристрій розімкнено).
    string Snapshot  = "";
    string OwnerName = "";
    // Чи Є в пристрою власник узагалі: без нього сторінка пропонує
    // ІНІЦІАЦІЮ, з чужим -- скидання.
    bool   Owned = false;
    // Секції нарізно: -1 -- секції немає. Стелі класу поруч, 0 -- безліміт.
    int    CarrierMarks = -1;
    int    CarrierNotes = -1;
    int    CarrierMaxMarks = 0;
    int    CarrierMaxNotes = 0;
    // Скільки одиниць на чипі; -1 -- невідомо (чужий род або старий запис).

    // --- замок ---
    bool HasPin   = false;
    bool Unlocked = true;
    bool AutoLock = true;
    // Сервер може заборонити вимикати автоблокування. Клієнту це треба, щоб
    // не малювати кнопку, яка завжди відмовляє.
    bool ForceAutoLock = false;

    // --- запечатаний пристрій ---
    //
    // Sealed і LockedOut -- РІЗНІ речі, хоч обидва означають «код не
    // допоможе»: перший через те, що коду ніхто не знає, другий через те, що
    // спроби вичерпані. Гравцеві треба сказати, котре з двох.
    bool  Sealed       = false;
    bool  HasDecryptor = false;
    bool  Cracking     = false;
    int   CrackLeftSec = 0;
    bool LockedOut = false;           // спроби вичерпані
    int  LockWaitS = 0;               // секунд до кінця блокування; 0 -- без вікна
    float LockAfterMinutes = 0;

    // --- сесія ---
    bool   Online     = false;        // епохи збігаються
    bool   SessionMine = false;       // сесія відкрита саме мною
    string SnapshotAt = "";           // коли знімок замерз; порожньо -- живий

    // --- прив'язка ---
    bool   DiscordLinked = false;
    string FirstSeen     = "";

    // --- радіація ---
    // Присилається лише якщо вставлено відповідний модуль. Від'ємне значення
    // означає «немає даних» і мусить малюватись саме так, а не нулем: нуль
    // означає «чисто», і брехати цим не можна.
    bool  HasRadiationProvider = false;
    float AmbientUSvH = -1;
    float DoseUSv     = -1;
    float DoseWarnUSv = -1;

    void OZ_PdaDeviceStatus()
    {
        Pages = new array<string>();
        Bays  = new array<ref OZ_BayInfo>();
    }
}

// --- дрібні операції, які клієнт надсилає на сервер ---
//
// Кожна -- окремий тип, а не мапа рядків: поле з назвою Pin у сигнатурі
// видно на очі, а ["pin"] у мапі не видно нікому, поки не зламається.

class OZ_PdaPinAttempt
{
    string Pin = "";
}

class OZ_PdaPinChange
{
    string OldPin = "";
    string NewPin = "";
}

class OZ_PdaFlagOp
{
    bool Value = false;
}

// --- сторінка «Контакти» ---

class OZ_ContactEntry
{
    string Name = "";

    // ХТО ЦЕ -- окремо від того, ЯК ЙОГО ЗВУТЬ.
    //
    // Раніше людину в списку впізнавали по імені, і це ламалось двічі. Двоє
    // з однаковим ім'ям -- а ім'я в DayZ вибирає сам гравець -- і «викреслити
    // з друзів» викреслювало першого-ліпшого з двох. А контакт, чиє ім'я ще
    // не встигло кешуватись, малювався як «---» і не піддавався взагалі
    // нічому: ні написати, ні прибрати.
    //
    // Ключ -- хеш steam-id, не сам id: клієнту нема чого знати steam-id своїх
    // контактів, а щоб відрізнити двох у ВЛАСНОМУ списку друзів, хеша
    // вистачає з головою.
    string Key = "";
    bool   Me   = false;   // це ти

    // Стан стосунків, рядком: "" -- ніхто, "friend" -- прийнятий друг,
    // "sent" -- я попросив і чекаю, "got" -- попросили мене,
    // "near" -- поруч, можна попросити.
    //
    // Рядком, а не набором булевих: станів п'ять і вони взаємовиключні, а
    // п'ять булевих дозволяють двадцять неможливих комбінацій.
    string Rel = "";

    // Фракція, вже людською назвою: клієнту нема чого знати id, а сервер уже
    // має під рукою і те, і те.
    string Faction = "";

    // КОЛІР фракції, готовим ARGB. Не слаг -- по слагу клієнт кольору не
    // дістане: OZ_Factions -- служба СЕРВЕРА, її таблиця на клієнті порожня,
    // і ColorARGB там повертає біле. Видно було одразу: смужка фракції
    // Боргу вийшла білою замість червоної.
    //
    // Та сама причина, що й у назви поруч: сервер уже має і те, і те.
    int FactionColor = 0;

    // Чи він зараз у Зоні. Раніше цього не було, бо в списку були самі
    // присутні; тепер контакт лишається в списку й офлайн, і мовчазний
    // порожній рядок означав би «поруч».
    bool Online = false;

    // Чи в межах простягнутої руки просто зараз. Просити в друзі можна лише
    // зблизька, і кнопку слід малювати лише тоді.
    bool Near = false;

    // Звання, посади й мітки -- ВЖЕ людськими назвами, як і фракція, і з тієї
    // ж причини: слаги -- внутрішня справа, а перекладати їх на екрані мусив
    // би кожен, хто малює.
    //
    // Приїжджали від моста з першого дня й не малювались НІДЕ: лідер фракції
    // на екрані був не відрізнити від новачка, а мітка «Механік» не давала
    // нічого й нікому. Знайдено аудитом, а не в грі.
    string Rank = "";
    ref array<string> Posts;
    ref array<string> Traits;

    // Чи він у МОЇЙ фракції. Окремим полем, а не порівнянням назв: назви
    // задає адмін, і дві однакові -- його право, а не привід зарахувати
    // чужого в свої.
    bool Mine = false;

    // Поля «схований» тут НЕМАЄ навмисно. Для чужих воно завжди false (бо
    // схований у список не потрапляє зовсім), а для себе відповідь уже є --
    // MeHidden у самому списку. Друге поле про те саме означало б два місця,
    // де це може розійтись.

    // Конструктор ПІСЛЯ полів. Оголошений перед ними, він валить компіляцію
    // всього модуля World, і повідомлення про це не згадує ані цей клас, ані
    // конструктор: сипляться «Bad type 'JsonFileLoader'» по всіх файлах, які
    // цей тип серіалізують.
    void OZ_ContactEntry()
    {
        Posts  = new array<string>();
        Traits = new array<string>();
    }
}

// Лічильника окремим полем НЕМАЄ навмисно: він дорівнює довжині списку, а
// будь-яке інше число підказало б, що когось приховано.
class OZ_ContactList
{
    bool MeHidden = false;

    // Чи я лідер своєї фракції -- від цього залежить, чи малювати лідерські
    // кнопки взагалі. Рішення сервера: клієнт про свої права не здогадується.
    bool MeLeader = false;

    // Запрошення до фракції, яке чекає на МЕНЕ. Порожня назва -- немає.
    //
    // Живе в списку, а не в записі контакту: воно про мене, а не про когось
    // із рядка, і показувати його треба незалежно від того, кого я обрав.
    string InviteFaction = "";
    string InviteFrom    = "";

    // Чи проекція ролей протухла. Той, хто малює, мусить показати ОСТАННЄ
    // ВІДОМЕ приглушеним, а не порожнє: порожнє читається як «одинак», і
    // робити такий висновок гра права не має.
    //
    // Обчислювалось із першого дня й не питалось ніде.
    bool Stale = false;

    // Читальня капсули: імена з дайджеста замороженого пристрою. Людей
    // ПАМ'ЯТАЮТЬ, а не бачать: ні присутності, ні дій.
    bool Frozen = false;

    ref array<ref OZ_ContactEntry> Entries;

    void OZ_ContactList()
    {
        Entries = new array<ref OZ_ContactEntry>();
    }
}

// --- сторінка «Фракція» ---

class OZ_FactionMember
{
    string Name = "";
    // Сталкерське звання -- особисте, поза фракцією.
    string Rank = "";
    // Внутрішньофракційне: підпис для екрана і слаг для арифметики
    // «наступне вище». Порожньо -- звання немає.
    string FRank   = "";
    string FRankId = "";
    bool Leader = false;
    bool Online = false;
    bool Me     = false;
}

class OZ_FactionState
{
    // Порожній slug -- одинак: сторінка чесно каже, що фракції немає.
    string Faction     = "";
    string FactionName = "";
    int    Color       = 0;
    string MyRank      = "";
    bool   MeLeader    = false;

    // Запрошення, що чекає САМЕ на мене.
    string InviteFaction = "";
    string InviteFrom    = "";

    ref array<ref OZ_FactionMember> Members;
    // Кого лідер може покликати: друзі поза фракцією, іменами.
    ref array<string> Candidates;

    // Драбина ЦІЄЇ фракції, знизу вгору: слаги й підписи поруч. Порожня --
    // звань у фракції не заводили, і кнопки підвищення нема сенсу малювати.
    ref array<string> RankIds;
    ref array<string> RankNames;

    void OZ_FactionState()
    {
        Members    = new array<ref OZ_FactionMember>();
        Candidates = new array<string>();
        RankIds    = new array<string>();
        RankNames  = new array<string>();
    }
}

// --- сторінка «Записки» ---
//
// Порожній Id означає «це нова записка». Так клієнту не треба знати, як
// сервер їх нумерує, і не треба другої операції «створити».

class OZ_Note
{
    string Id        = "";
    string Title     = "";
    string Body      = "";
    string CreatedAt = "";
    string EditedAt  = "";
}

class OZ_NoteRef
{
    string Id = "";
}

// --- сторінка «Карта» ---
//
// Позиції рядком "x y z" -- у тому ж вигляді, у якому їх віддає vector, і в
// тому ж, у якому їх читає ToVector(). Окремих трьох полів тут не треба.

class OZ_MapBeacon
{
    string Name = "";
    string Pos  = "";
}

class OZ_MapState
{
    string SelfPos = "";

    // Антена -- умова і прийому, і передачі. Без неї маячків немає взагалі,
    // і це ОКРЕМИЙ стан, а не порожній список.
    bool  HasAntenna    = false;
    float AntennaRangeM = 0;

    // Свій режим транспондера: off | public | friends | contacts.
    string TransponderMode = "off";

    ref array<ref OZ_MapBeacon> Beacons;

    // Мітки цього ПРИСТРОЮ і скільки їх іще влізе. Другий лічильник тут не
    // зайвий: «більше не влізе» гравець має дізнатись до того, як натисне.
    ref array<ref OZ_MapMarker> Markers;
    int MarkerLimit = 0;

    // Маршрут пристрою: впорядковані копії міток. Активація -- справа
    // клієнта; тут лише дані.
    ref array<ref OZ_MapMarker> Route;

    void OZ_MapState()
    {
        Beacons = new array<ref OZ_MapBeacon>();
        Markers = new array<ref OZ_MapMarker>();
        Route   = new array<ref OZ_MapMarker>();
    }
}

class OZ_TransponderOp
{
    string Mode = "off";
}

// Посилання на людину ІМЕНЕМ, а не Steam64. Клієнт чужого id не бачить і не
// має бачити; сервер сам вирішує, кому це ім'я належить -- і серед кого саме
// шукати (поруч, у друзях, серед запитів).
// Посилання на людину або на рядок тексту.
//
// Key -- коли йдеться про КОНТАКТА (див. OZ_ContactEntry.Key). Name лишається
// для того, що ім'ям і є насправді: назви нової групи, наприклад.
class OZ_NameRef
{
    string Name = "";
    string Key  = "";
}

// --- мітки ---
//
// Мітки живуть НА ПРИСТРОЇ, а не в акаунті -- на відміну від записок. Через
// це межа Limits.Markers у профілі щось означає (кращий КПК тримає більше),
// через це має сенс носій даних, і через це вкрадений КПК віддає чужі
// схованки. Усе три -- навмисно.

class OZ_MapMarker
{
    string Id   = "";
    string Name = "";
    string Pos  = "";

    // Опис -- довше за назву й не для карти: на карті він був би кашею.
    // Живе в списку міток і редагується там само. Старі записи без поля
    // читаються як порожній опис -- JsonFileLoader незнайоме поле не чіпає,
    // а відсутнє лишає замовчуванням.
    string Desc = "";
}

class OZ_MarkerList
{
    ref array<ref OZ_MapMarker> Items;

    void OZ_MarkerList()
    {
        Items = new array<ref OZ_MapMarker>();
    }
}

class OZ_MarkerRef
{
    string Id = "";
}

// --- носій даних ---
//
// Що писати на чип. Kind -- "markers" або "notes"; порожні списки
// означають «усе, що є».
// Вiдповiдь iмпорту: скiльки взяли проти скiльки лежало. Рiзниця мiж ними
// -- те, що НЕ влiзло, i гравець мусить це побачити, а не почути "Done.".
class OZ_CarrierTaken
{
    int Taken = 0;
    int Total = 0;
}

class OZ_CarrierWriteOp
{
    string Kind = "";
}

// --- сторінка «Зв'язок» ---
//
// По проводу їдуть ІМЕНА, а не Steam64: клієнт чужих id не бачить ніде, і
// чат тут не виняток.

// Рядок переліку розмов. Лічильника повідомлень тут немає навмисно: у
// Discord їх стільки, скільки їх там є, а міст тримає лише хвіст -- назвати
// довжину хвоста «кількістю повідомлень» означало б збрехати.
class OZ_ChatHead
{
    string Id       = "";
    string Kind     = "direct";
    string Title    = "";
    string Desc     = "";
    string LastAt   = "";
    string LastText = "";
}

// Запрошення до групи, яке чекає на мене. Прийняти чи відхилити --
// МІЙ клік, а не чужий: у групу ніхто не потрапляє мовчки.
class OZ_ChatInvite
{
    string Id    = "";
    string Title = "";
    string From  = "";
}

class OZ_ChatList
{
    // Читальня капсули: список приїхав зрізом, нового не завести.
    bool Frozen = false;
    ref array<ref OZ_ChatHead> Items;
    ref array<ref OZ_ChatInvite> Invites;

    void OZ_ChatList()
    {
        Items   = new array<ref OZ_ChatHead>();
        Invites = new array<ref OZ_ChatInvite>();
    }
}

class OZ_ChatLine
{
    string At   = "";
    // Слід автора для СЕРВЕРА: міст кладе сюди Steam64, сервер міняє
    // його на колір фракції і СТИРАЄ -- клієнтові чужі id не дістаються.
    string AUid = "";
    // ARGB фракції автора; 0 -- без фарби.
    int WhoColor = 0;
    string Who  = "";
    string Text = "";
    bool   Mine = false;
}

class OZ_ChatView
{
    string Id    = "";
    string Kind  = "direct";
    string Title = "";
    string Desc  = "";
    // Чи є що вантажити ГЛИБШЕ, і якір найстарішого показаного рядка.
    // Якір непрозорий: клієнт лише повертає його в "older" як є.
    bool   More = false;
    string Before = "";
    // Читальня капсули: рядки зрізані по заморозці, поле вводу мертве.
    bool   Frozen = false;
    // Чи Я створив цю групу: ключ групи назавжди носить ім'я засновника,
    // і лише він її видаляє; решта -- виходять.
    bool   Owner = false;
    ref array<ref OZ_ChatLine> Lines;
    ref array<string> Members;

    void OZ_ChatView()
    {
        Lines   = new array<ref OZ_ChatLine>();
        Members = new array<string>();
    }
}

class OZ_ChatRef
{
    string Id = "";
}

class OZ_ChatSend
{
    string Id   = "";
    string Text = "";

    // Тільки для «Зони»: сказати в ефір без імені. Міст сам відмовить
    // будь-якій іншій розмові -- там співрозмовник обирав, З КИМ говорить.
    bool Anon = false;
}

class OZ_ChatAdd
{
    string Id   = "";
    string Name = "";
    string Key  = "";
}

// Один запис на носії, на який вказує гравець: секція і місце в ній.
class OZ_CarrierItemRef
{
    string Kind = "";   // "mark" | "note"
    int    Index = -1;
}

// Кого можна покликати в групу: імена контактів гравця. Відповідь сторінки
// чату, бо саме їй потрібна -- меню маршрутизує відповіді за сторінкою.
class OZ_ChatInvitees
{
    ref array<string> Names;

    void OZ_ChatInvitees()
    {
        Names = new array<string>();
    }
}

// Назва й опис групи одним листом: Id порожній -- створити, заданий --
// правити існуючу.
class OZ_ChatGroupSpec
{
    string Id   = "";
    string Name = "";
    string Desc = "";
}

// «Дай старіше»: якір -- Before із попередньої відповіді.
class OZ_ChatOlderReq
{
    string Id     = "";
    string Before = "";
}

// Знімок для капсули часу: що замерзне на пристрої, коли власник заведе
// новий. Пише сервер із живої сесії, читає сторінка пристрою офлайн-капсули.
class OZ_PdaSnapshot
{
    string Owner   = "";
    string Faction = "";
    ref array<string> Contacts;

    void OZ_PdaSnapshot()
    {
        Contacts = new array<string>();
    }
}
