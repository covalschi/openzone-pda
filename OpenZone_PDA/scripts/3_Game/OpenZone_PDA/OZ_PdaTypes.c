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
