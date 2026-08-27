// Як КПК тримають у руці.
//
// Без цього він лежав у кулаці як яблуко: Inventory_Base зареєстрований із
// позою "apple.anm" (dayzplayercfgbase.c:373), і будь-який предмет, для якого
// ніхто не сказав інакше, дістає саме її.
//
// ЯК ЦЕ ВЗАГАЛІ ВЛАШТОВАНО, бо з конфіга предмета цього не видно й шукається
// довго. Поза береться НЕ з класу предмета й не з моделі: рушій тримає
// таблицю «клас предмета -> робочий простір анімацій + IK-поза», яку наповнює
// DayZPlayerTypeRegisterItems. Ані ItemGPS, ані GPSReceiver у конфізі не мають
// жодного ключа про позу -- усе вирішує цей рядок:
//
//   AddItemInHandsProfileIK("GPSReceiver",
//       "dz/anims/workspaces/player/player_main/props/player_main_1h_GPSReciever.asi",
//       toolsOneHanded, "dz/anims/anm/player/ik/gear/GPSReciever.anm")
//
// Беремо ту саму пару: наш прилад -- це прилад розміром із GPS, який дивляться
// на витягнутій руці, і власної анімації в нас поки немає. Коли приїде своя
// модель з dayz-3d, міняється лише шлях .anm.
//
// РЕЄСТРУЄМО БАЗУ, а не три класи: пошук у таблиці йде за спадкуванням --
// саме тому один запис "Inventory_Base" накриває всю гру.
//
// ModItemRegisterCallbacks.RegisterCustom -- офіційний гачок для цього
// (dayzplayercfgbase.c:1487), і він єдиний, який рушій кличе ПІСЛЯ всіх
// ванільних реєстрацій. Тому наш запис не можна перебити випадково.

modded class ModItemRegisterCallbacks
{
    private static const string OZ_WORKSPACE =
        "dz/anims/workspaces/player/player_main/props/player_main_1h_GPSReciever.asi";

    private static const string OZ_POSE =
        "dz/anims/anm/player/ik/gear/GPSReciever.anm";

    override void RegisterCustom(DayZPlayerType pType)
    {
        super.RegisterCustom(pType);

        DayzPlayerItemBehaviorCfg oneHanded = new DayzPlayerItemBehaviorCfg;
        oneHanded.SetToolsOneHanded();

        pType.AddItemInHandsProfileIK("OZ_PDA_Base", OZ_WORKSPACE, oneHanded, OZ_POSE);
    }
}
