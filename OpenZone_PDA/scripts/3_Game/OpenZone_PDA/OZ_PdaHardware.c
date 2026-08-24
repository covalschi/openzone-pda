// Залізо, яке вставляється в КПК.
//
// Три різні речі, а не один список слотів:
//
//   ЖИВЛЕННЯ   -- батарея. Слот ванільний, рушій сам втикає в неї пристрій.
//   СХОВИЩЕ    -- носій даних. Свій слот: це вміст, а не здатність.
//   МОДУЛІ     -- антена, радіометр, дозиметр і що завгодно від інших модів.
//                 Ділять ОБМЕЖЕНЕ число відсіків.
//
// Відсіків обмежено навмисно: це головний важіль тиру. У ПДА новачка один, у
// долговського три -- і гравець сам вирішує, що нести: далекий зв'язок чи
// лічильник Гейгера. Список слотів без обмеження такого вибору не дає.
//
// Що саме влазить у відсік і що це дає -- таблиця класнеймів нижче. Пізнаємо
// ЗВІРКОЮ КЛАСНЕЙМА, ніколи спорідненістю: тоді модуль може принести будь-який
// мод, а адмін -- навести КПК на предмет, про який ми не чули.

class OZ_ModuleSpec
{
    string ClassName   = "";
    string DisplayName = "";

    // Що це за прилад. Рядком, а не числом: мод-постачальник не мусить знати
    // наших констант, а адмін бачить у JSON слово, а не код.
    //
    //   "antenna"     -- вмикає далекий зв'язок, дає радіус
    //   "radiometer"  -- лічильник Гейгера: зовнішнє поле тут і зараз
    //   "dosimeter"   -- ін'єкційний: накопичена доза в тілі
    //   будь-що інше  -- чужий модуль, КПК просто вмикає його сторінки
    string Kind = "";

    // Радіус упевненого прийому в метрах. Має сенс лише для "antenna".
    // Нуль означає «покриття задає щось інше» -- наприклад стаціонарна вежа.
    float RangeM = 0;

    // Скільки цей модуль додає до витрати живлення, у частках від базової.
    // Радіометр, що безперервно міряє, їсть батарею -- і це має бути видно.
    float PowerFactor = 1.0;

    // Які сторінки модуль вмикає. Антена вмикає "radio", радіометр -- свою
    // шкалу; поле загальне, щоб чужий модуль міг увімкнути свою сторінку.
    ref array<string> EnablesPages;
}

class OZ_CarrierSpec
{
    string ClassName   = "";
    string DisplayName = "";
    // Вид вмісту за замовчуванням. Збігається з id сторінки, яка вміє його
    // читати: "markers", "chatlog" і так далі.
    string DefaultKind = "";
    // Чи можна перезаписати носій із КПК. Одноразовий чип із чужої схованки
    // перезаписувати не можна -- у цьому половина його цінності.
    bool   Writable    = true;
}

class OZ_PdaHardwareConfig : OZ_ConfigBase
{
    ref array<ref OZ_ModuleSpec>  Modules;
    ref array<ref OZ_CarrierSpec> Carriers;

    override int LatestVersion()
    {
        return 1;
    }

    override void LoadDefaults()
    {
        Version  = LatestVersion();
        Modules  = new array<ref OZ_ModuleSpec>();
        Carriers = new array<ref OZ_CarrierSpec>();

        OZ_ModuleSpec radio = new OZ_ModuleSpec();
        radio.ClassName    = "OZ_Module_Radiometer";
        radio.DisplayName  = "#STR_OZ_MOD_RADIOMETER";
        radio.Kind         = "radiometer";
        radio.PowerFactor  = 1.4;
        radio.EnablesPages = new array<string>();
        Modules.Insert(radio);

        OZ_ModuleSpec dose = new OZ_ModuleSpec();
        dose.ClassName    = "OZ_Module_Dosimeter";
        dose.DisplayName  = "#STR_OZ_MOD_DOSIMETER";
        dose.Kind         = "dosimeter";
        // Ін'єкційний датчик живиться сам і батарею КПК майже не чіпає.
        dose.PowerFactor  = 1.05;
        dose.EnablesPages = new array<string>();
        Modules.Insert(dose);

        // Базова антена ЙДЕ З КПК, а не з мода рації. Транспондер -- функція
        // самого пристрою, і ставити його в залежність від ще не написаного
        // мода означало б віддати КПК без того, заради чого його носять.
        //
        // Далекі антени й вежі приносить OpenZone Radio; вони просто мають
        // більший RangeM і перекривають цю.
        OZ_ModuleSpec ant = new OZ_ModuleSpec();
        ant.ClassName    = "OZ_Module_Antenna";
        ant.DisplayName  = "#STR_OZ_MOD_ANTENNA";
        ant.Kind         = "antenna";
        ant.RangeM       = 500;
        // Передавач їсть більше за будь-який датчик, і це має бути видно по
        // батареї.
        ant.PowerFactor  = 1.6;
        ant.EnablesPages = new array<string>();
        Modules.Insert(ant);

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

        if (!Modules)
            Modules = new array<ref OZ_ModuleSpec>();
        if (!Carriers)
            Carriers = new array<ref OZ_CarrierSpec>();

        for (int i = 0; i < Modules.Count(); i++)
        {
            OZ_ModuleSpec m = Modules[i];

            if (!GetGame().ConfigIsExisting("CfgVehicles " + m.ClassName))
            {
                OZ_Log.Warn("module class \"" + m.ClassName + "\" is not in CfgVehicles - is its mod loaded?");
                warnings++;
            }

            if (!m.EnablesPages)
                m.EnablesPages = new array<string>();

            if (m.Kind == "")
            {
                OZ_Log.Warn("module \"" + m.ClassName + "\" has no Kind - it will attach but do nothing");
                warnings++;
            }

            if (m.RangeM < 0)
            {
                OZ_Log.Warn("module \"" + m.ClassName + "\" has a negative RangeM, clamped to 0");
                m.RangeM = 0;
                warnings++;
            }

            if (m.PowerFactor < 0)
            {
                OZ_Log.Warn("module \"" + m.ClassName + "\" has a negative PowerFactor, clamped to 1");
                m.PowerFactor = 1.0;
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

    static OZ_PdaHardwareConfig Get()      { return s_Cfg; }

    static int ModuleCount()
    {
        if (!s_Cfg)
            return 0;
        return s_Cfg.Modules.Count();
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

    static OZ_ModuleSpec ModuleFor(string cls)
    {
        if (!s_Cfg)
            return null;

        for (int i = 0; i < s_Cfg.Modules.Count(); i++)
        {
            if (s_Cfg.Modules[i].ClassName == cls)
                return s_Cfg.Modules[i];
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
