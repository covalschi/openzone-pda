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

    // --- живлення ---
    bool  Powered  = false;
    float Charge01 = 0;

    // --- залізо ---
    ref array<ref OZ_BayInfo> Bays;
    string CarrierClass = "";
    string CarrierKind  = "";
    bool   CarrierWritten = false;

    // --- замок ---
    bool HasPin   = false;
    bool Unlocked = true;
    bool AutoLock = true;
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
