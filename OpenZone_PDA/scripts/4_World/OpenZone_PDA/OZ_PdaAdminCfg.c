// Аплаєри конфігів КПК для адмінської консолі ядра.
//
// ЖИВУТЬ У 4_World, а не поруч із самим конфігом: базовий OZ_AdminCfgApplier
// оголошений ядром у 4_World, і 3_Game його ще не бачить -- модулі скриптів
// компілюються ярусами (зміряно цим самим файлом 2026-08-30).

// Аплаєри для адмінської консолі: розбір у тимчасовий об'єкт, збереження
// через загальний лоадер (він робить .bak), потім ServerLoad -- живий конфіг
// перечитується з уже перевіреного диска повним конвеєром валідації.
class OZ_PdaTuningApplier : OZ_AdminCfgApplier
{
    override bool Apply(string json)
    {
        OZ_PdaTuning tmp;
        string err;
        if (!JsonFileLoader<OZ_PdaTuning>.LoadData(json, tmp, err) || !tmp)
        {
            OZ_Log.Warn("admin: Tuning.json rejected: " + err);
            return false;
        }

        OZ_ConfigLoader<OZ_PdaTuning>.Save(OZ_Const.PROFILE_DIR + "\\OZ_PDA_Tuning.json", "Tuning", tmp);
        OZ_PdaTuning.ServerLoad();
        return true;
    }
}

class OZ_PdaProfilesApplier : OZ_AdminCfgApplier
{
    override bool Apply(string json)
    {
        OZ_PdaProfilesConfig tmp;
        string err;
        if (!JsonFileLoader<OZ_PdaProfilesConfig>.LoadData(json, tmp, err) || !tmp)
        {
            OZ_Log.Warn("admin: Profiles.json rejected: " + err);
            return false;
        }

        OZ_ConfigLoader<OZ_PdaProfilesConfig>.Save(OZ_PdaConst.PROFILES, "Profiles", tmp);
        OZ_PdaProfiles.ServerLoad();
        return true;
    }
}

class OZ_PdaHardwareApplier : OZ_AdminCfgApplier
{
    override bool Apply(string json)
    {
        OZ_PdaHardwareConfig tmp;
        string err;
        if (!JsonFileLoader<OZ_PdaHardwareConfig>.LoadData(json, tmp, err) || !tmp)
        {
            OZ_Log.Warn("admin: Hardware.json rejected: " + err);
            return false;
        }

        OZ_ConfigLoader<OZ_PdaHardwareConfig>.Save(OZ_PdaConst.HARDWARE, "Hardware", tmp);
        OZ_PdaHardware.ServerLoad();
        return true;
    }
}
