// Залізо, яке вставляється в КПК: антени й носії даних.
//
// Таблиця класнеймів у JSON, за тим самим правилом, що й профілі пристроїв:
// пізнаємо предмет ЗВІРКОЮ КЛАСНЕЙМА, а не спорідненістю. Тоді антену може
// принести мод рації, носій -- контент-мод, а адмін вільний навести КПК на
// предмет із мода, про який ми ніколи не чули.
//
// Антена -- це те, що ВМИКАЄ в КПК радіо. Без неї сторінка рації недоступна,
// хоч би мод рації й стояв: пристрій без антени нічого не ловить. Радіус
// покриття задає конкретна антена, і саме тут -- різниця між гілкою дроту й
// нормальним штирем.

class OZ_AntennaSpec
{
    string ClassName   = "";
    string DisplayName = "";
    // Радіус упевненого прийому в метрах. Нуль означає «антена є, але
    // покриття задає щось інше» -- наприклад, стаціонарна вежа.
    float  RangeM      = 500;
    // Скільки ця антена додає до витрати живлення, у частках від базової.
    float  PowerFactor = 1.0;
    // Які сторінки вона вмикає. Зазвичай одна -- "radio", -- але поле
    // загальне: антена-сканер могла б вмикати й щось своє.
    ref array<string> EnablesPages;
}

class OZ_CarrierSpec
{
    string ClassName   = "";
    string DisplayName = "";
    // Вид вмісту за замовчуванням для щойно створеного носія. Збігається з
    // id сторінки, яка вміє його читати: "markers", "chatlog" і так далі.
    string DefaultKind = "";
    // Чи можна перезаписати носій із КПК. Одноразовий чип із чужого схрону
    // перезаписувати не можна -- у цьому половина його цінності.
    bool   Writable    = true;
}

class OZ_PdaHardwareConfig : OZ_ConfigBase
{
    ref array<ref OZ_AntennaSpec> Antennas;
    ref array<ref OZ_CarrierSpec> Carriers;

    override int LatestVersion()
    {
        return 1;
    }

    override void LoadDefaults()
    {
        Version  = LatestVersion();
        Antennas = new array<ref OZ_AntennaSpec>();
        Carriers = new array<ref OZ_CarrierSpec>();

        // Порожньо навмисно. Антени приносить OpenZone Radio, і оголошувати
        // їх тут означало б попереджати про відсутні класи на кожному сервері
        // без мода рації.

        OZ_CarrierSpec chip = new OZ_CarrierSpec();
        chip.ClassName   = "OZ_DataCarrier_Chip";
        chip.DisplayName = "#STR_OZ_CARRIER_CHIP";
        chip.DefaultKind = "";
        chip.Writable    = true;
        Carriers.Insert(chip);
    }

    override bool Migrate(int from)
    {
        Version = LatestVersion();
        return true;
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Antennas)
            Antennas = new array<ref OZ_AntennaSpec>();
        if (!Carriers)
            Carriers = new array<ref OZ_CarrierSpec>();

        for (int i = 0; i < Antennas.Count(); i++)
        {
            OZ_AntennaSpec a = Antennas[i];

            if (!GetGame().ConfigIsExisting("CfgVehicles " + a.ClassName))
            {
                OZ_Log.Warn("antenna class \"" + a.ClassName + "\" is not in CfgVehicles - is its mod loaded?");
                warnings++;
            }

            if (!a.EnablesPages)
                a.EnablesPages = new array<string>();

            if (a.RangeM < 0)
            {
                OZ_Log.Warn("antenna \"" + a.ClassName + "\" has a negative RangeM, clamped to 0");
                a.RangeM = 0;
                warnings++;
            }
        }

        for (int c = 0; c < Carriers.Count(); c++)
        {
            if (!GetGame().ConfigIsExisting("CfgVehicles " + Carriers[c].ClassName))
            {
                OZ_Log.Warn("carrier class \"" + Carriers[c].ClassName + "\" is not in CfgVehicles - is its mod loaded?");
                warnings++;
            }
        }
    }
}

class OZ_PdaHardware
{
    private static ref OZ_PdaHardwareConfig s_Cfg;

    static OZ_PdaHardwareConfig Get()
    {
        return s_Cfg;
    }

    static int AntennaCount()
    {
        if (!s_Cfg)
            return 0;
        return s_Cfg.Antennas.Count();
    }

    static int CarrierCount()
    {
        if (!s_Cfg)
            return 0;
        return s_Cfg.Carriers.Count();
    }

    static void ServerLoad()
    {
        s_Cfg = new OZ_PdaHardwareConfig();
        OZ_ConfigLoader<OZ_PdaHardwareConfig>.Load(OZ_PdaConst.HARDWARE, "Hardware", s_Cfg);
    }

    static OZ_AntennaSpec AntennaFor(string cls)
    {
        if (!s_Cfg)
            return null;

        for (int i = 0; i < s_Cfg.Antennas.Count(); i++)
        {
            if (s_Cfg.Antennas[i].ClassName == cls)
                return s_Cfg.Antennas[i];
        }
        return null;
    }

    static OZ_CarrierSpec CarrierFor(string cls)
    {
        if (!s_Cfg)
            return null;

        for (int i = 0; i < s_Cfg.Carriers.Count(); i++)
        {
            if (s_Cfg.Carriers[i].ClassName == cls)
                return s_Cfg.Carriers[i];
        }
        return null;
    }
}
