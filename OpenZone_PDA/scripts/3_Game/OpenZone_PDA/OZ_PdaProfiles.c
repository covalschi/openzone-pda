// Профілі пристроїв: що вміє конкретний КПК.
//
// Тир задає ПРЕДМЕТ, а не гравець: ПДА новачка й ПДА долговця -- різні
// класнейми з різними наборами сторінок, лімітів і живлення.
//
// ClassNames -- список, і в ньому може стояти класнейм БУДЬ-ЯКОГО мода.
// Пізнаємо пристрій звіркою obj.GetType() зі списком, і НІКОЛИ спорідненістю
// з нашим класом: інакше адмін не зміг би навести мод на чужий предмет, а
// це половина сенсу «максимально гнучкого».

class OZ_PdaLimits
{
    int Markers    = 10;
    int Friends    = 20;
    int GroupChats = 2;
}

class OZ_PdaProfile
{
    string            Id          = "";
    ref array<string> ClassNames;
    string            DisplayName = "";
    ref array<string> Pages;
    bool              RequiresPower    = true;
    ref array<string> BatteryClassNames;
    float             PowerDrainPerMin = 0.5;
    ref OZ_PdaLimits  Limits;
    string            Theme       = "stalker2";

    // Скільки модульних відсіків видно на цій моделі. ГОЛОВНИЙ важіль тиру:
    // один відсік змушує вибирати між далеким зв'язком і лічильником Гейгера,
    // три дозволяють нести все. Стеля -- MODULE_SLOTS_MAX, бо слоти не
    // додаються в рантаймі.
    int ModuleSlots = 1;

    // --- замок ---
    // Через скільки хвилин після того, як пристрій прибрали з рук, він
    // замикається сам. Нуль вимикає автоблокування для цієї моделі зовсім.
    float LockAfterMinutes = 5;
    // Сервер забороняє гравцеві вимикати автоблокування. Для серверів, де
    // залутаний КПК має лишатись цінністю, а не безкоштовним трофеєм.
    bool  ForceAutoLock    = false;
    // Сторінки, які просять код ЩЕ РАЗ, навіть на відімкненому пристрої.
    // Порожньо -- нічого не просить.
    ref array<string> PinProtectedPages;
}

class OZ_PdaVirtualDevice
{
    bool              Enabled = false;
    ref array<string> Pages;
    ref array<string> Factions;
}

class OZ_PdaProfilesConfig : OZ_ConfigBase
{
    ref array<ref OZ_PdaProfile> Profiles;
    ref OZ_PdaVirtualDevice      VirtualDevice;

    override int LatestVersion()
    {
        return OZ_PdaConst.SCHEMA_PROFILES;
    }

    override void LoadDefaults()
    {
        Version  = LatestVersion();
        Profiles = new array<ref OZ_PdaProfile>();

        OZ_PdaProfile p = new OZ_PdaProfile();
        p.Id          = "novice";
        p.DisplayName = "#STR_OZ_PDA_NOVICE";

        p.ClassNames = new array<string>();
        p.ClassNames.Insert("OZ_PDA_Novice");

        p.Pages = new array<string>();
        p.Pages.Insert(OZ_PdaConst.PAGE_DEVICE);
        p.Pages.Insert(OZ_PdaConst.PAGE_QUESTS);

        p.BatteryClassNames = new array<string>();
        p.BatteryClassNames.Insert("Battery9V");

        p.Limits            = new OZ_PdaLimits();
        p.PinProtectedPages = new array<string>();
        Profiles.Insert(p);

        OZ_PdaProfile a = new OZ_PdaProfile();
        a.Id          = "advanced";
        a.DisplayName = "#STR_OZ_PDA_ADVANCED";

        a.ClassNames = new array<string>();
        a.ClassNames.Insert("OZ_PDA_Advanced");

        a.Pages = new array<string>();
        a.Pages.Insert(OZ_PdaConst.PAGE_DEVICE);
        a.Pages.Insert(OZ_PdaConst.PAGE_QUESTS);

        a.BatteryClassNames = new array<string>();
        a.BatteryClassNames.Insert("Battery9V");

        a.Limits            = new OZ_PdaLimits();
        a.Limits.Markers    = 40;
        a.Limits.Friends    = 60;
        a.Limits.GroupChats = 8;
        a.PinProtectedPages = new array<string>();
        a.ModuleSlots       = OZ_PdaConst.MODULE_SLOTS_MAX;
        a.LockAfterMinutes  = 5;
        Profiles.Insert(a);

        VirtualDevice = new OZ_PdaVirtualDevice();
        VirtualDevice.Pages    = new array<string>();
        VirtualDevice.Factions = new array<string>();
    }

    override bool Migrate(int from)
    {
        Version = LatestVersion();
        return true;
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Profiles)
            Profiles = new array<ref OZ_PdaProfile>();
        if (!VirtualDevice)
            OZ_PdaProfile a = new OZ_PdaProfile();
        a.Id          = "advanced";
        a.DisplayName = "#STR_OZ_PDA_ADVANCED";

        a.ClassNames = new array<string>();
        a.ClassNames.Insert("OZ_PDA_Advanced");

        a.Pages = new array<string>();
        a.Pages.Insert(OZ_PdaConst.PAGE_DEVICE);
        a.Pages.Insert(OZ_PdaConst.PAGE_QUESTS);

        a.BatteryClassNames = new array<string>();
        a.BatteryClassNames.Insert("Battery9V");

        a.Limits            = new OZ_PdaLimits();
        a.Limits.Markers    = 40;
        a.Limits.Friends    = 60;
        a.Limits.GroupChats = 8;
        a.PinProtectedPages = new array<string>();
        a.ModuleSlots       = OZ_PdaConst.MODULE_SLOTS_MAX;
        a.LockAfterMinutes  = 5;
        Profiles.Insert(a);

        VirtualDevice = new OZ_PdaVirtualDevice();

        for (int i = 0; i < Profiles.Count(); i++)
        {
            OZ_PdaProfile p = Profiles[i];

            if (!p.ClassNames || p.ClassNames.Count() == 0)
            {
                OZ_Log.Warn("profile \"" + p.Id + "\" has no ClassNames - nothing will ever match it");
                warnings++;
            }

            for (int c = 0; p.ClassNames && c < p.ClassNames.Count(); c++)
            {
                // Лише існування в конфізі гри. Спорідненість не перевіряємо
                // навмисно -- див. коментар угорі файлу.
                if (!GetGame().ConfigIsExisting("CfgVehicles " + p.ClassNames[c]))
                {
                    string wc = "profile \"" + p.Id;
                    wc += "\": class \"" + p.ClassNames[c];
                    wc += "\" is not in CfgVehicles";
                    OZ_Log.Warn(wc);
                    warnings++;
                }
            }

            if (!p.Pages)
                p.Pages = new array<string>();

            for (int g = 0; g < p.Pages.Count(); g++)
            {
                if (!OZ_PageRegistry.Has(p.Pages[g]))
                {
                    string wp = "profile \"" + p.Id;
                    wp += "\": page \"" + p.Pages[g];
                    wp += "\" is not registered";
                    OZ_Log.Warn(wp);
                    warnings++;
                }
            }

            if (!p.Limits)
                p.Limits = new OZ_PdaLimits();
            if (!p.BatteryClassNames)
                p.BatteryClassNames = new array<string>();
            if (!p.PinProtectedPages)
                p.PinProtectedPages = new array<string>();

            if (p.ModuleSlots < 0 || p.ModuleSlots > OZ_PdaConst.MODULE_SLOTS_MAX)
            {
                string wm = "profile \"" + p.Id;
                wm += "\" asks for " + p.ModuleSlots.ToString();
                wm += " module bays; the config declares at most ";
                wm += OZ_PdaConst.MODULE_SLOTS_MAX.ToString();
                OZ_Log.Warn(wm);
                p.ModuleSlots = Math.Clamp(p.ModuleSlots, 0, OZ_PdaConst.MODULE_SLOTS_MAX);
                warnings++;
            }

            if (p.LockAfterMinutes < 0)
            {
                OZ_Log.Warn("profile \"" + p.Id + "\" has a negative LockAfterMinutes, clamped to 0");
                p.LockAfterMinutes = 0;
                warnings++;
            }
        }
    }
}

class OZ_PdaProfiles
{
    private static ref OZ_PdaProfilesConfig s_Cfg;

    static OZ_PdaProfilesConfig Get()
    {
        return s_Cfg;
    }

    static int Count()
    {
        if (!s_Cfg)
            return 0;
        return s_Cfg.Profiles.Count();
    }

    static void ServerLoad()
    {
        s_Cfg = new OZ_PdaProfilesConfig();
        OZ_ConfigLoader<OZ_PdaProfilesConfig>.Load(OZ_PdaConst.PROFILES, "Profiles", s_Cfg);
    }

    static OZ_PdaProfile ForClass(string cls)
    {
        if (!s_Cfg)
            return null;

        for (int i = 0; i < s_Cfg.Profiles.Count(); i++)
        {
            array<string> names = s_Cfg.Profiles[i].ClassNames;
            for (int c = 0; names && c < names.Count(); c++)
            {
                if (names[c] == cls)
                    return s_Cfg.Profiles[i];
            }
        }
        return null;
    }
}
