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
    string CarrierKind  = "";
    bool   CarrierWritten = false;

    // --- замок ---
    bool HasPin   = false;
    bool Unlocked = true;
    bool AutoLock = true;
    // Сервер може заборонити вимикати автоблокування. Клієнту це треба, щоб
    // не малювати кнопку, яка завжди відмовляє.
    bool ForceAutoLock = false;
    bool LockedOut = false;           // спроби вичерпані
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
    bool   Me   = false;   // це ти

    // Стан стосунків, рядком: "" -- ніхто, "friend" -- прийнятий друг,
    // "sent" -- я попросив і чекаю, "got" -- попросили мене,
    // "near" -- поруч, можна попросити.
    //
    // Рядком, а не набором булевих: станів п'ять і вони взаємовиключні, а
    // п'ять булевих дозволяють двадцять неможливих комбінацій.
    string Rel = "";

    // Чи в межах простягнутої руки просто зараз. Просити в друзі можна лише
    // зблизька, і кнопку слід малювати лише тоді.
    bool Near = false;

    // Поля «схований» тут НЕМАЄ навмисно. Для чужих воно завжди false (бо
    // схований у список не потрапляє зовсім), а для себе відповідь уже є --
    // MeHidden у самому списку. Друге поле про те саме означало б два місця,
    // де це може розійтись.
}

// Лічильника окремим полем НЕМАЄ навмисно: він дорівнює довжині списку, а
// будь-яке інше число підказало б, що когось приховано.
class OZ_ContactList
{
    bool MeHidden = false;
    ref array<ref OZ_ContactEntry> Entries;

    void OZ_ContactList()
    {
        Entries = new array<ref OZ_ContactEntry>();
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

    void OZ_MapState()
    {
        Beacons = new array<ref OZ_MapBeacon>();
        Markers = new array<ref OZ_MapMarker>();
    }
}

class OZ_TransponderOp
{
    string Mode = "off";
}

// Посилання на людину ІМЕНЕМ, а не Steam64. Клієнт чужого id не бачить і не
// має бачити; сервер сам вирішує, кому це ім'я належить -- і серед кого саме
// шукати (поруч, у друзях, серед запитів).
class OZ_NameRef
{
    string Name = "";
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

// --- сторінка «Зв'язок» ---
//
// По проводу їдуть ІМЕНА, а не Steam64: клієнт чужих id не бачить ніде, і
// чат тут не виняток.

class OZ_ChatMsg
{
    string At       = "";
    string FromUid  = "";   // лишається на СЕРВЕРІ, клієнту не їде
    string FromName = "";
    string Text     = "";
}

class OZ_ChatHead
{
    string Id       = "";
    string Kind     = "direct";
    string Title    = "";
    int    Count    = 0;
    string LastAt   = "";
    string LastText = "";
}

class OZ_ChatList
{
    ref array<ref OZ_ChatHead> Items;

    void OZ_ChatList()
    {
        Items = new array<ref OZ_ChatHead>();
    }
}

class OZ_ChatLine
{
    string At   = "";
    string Who  = "";
    string Text = "";
    bool   Mine = false;
}

class OZ_ChatView
{
    string Id    = "";
    string Kind  = "direct";
    string Title = "";
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
}

class OZ_ChatAdd
{
    string Id   = "";
    string Name = "";
}
