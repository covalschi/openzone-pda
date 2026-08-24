class OZ_PdaConst
{
    static const string PROFILES = "$profile:OpenZone\\Profiles.json";

    static const string HARDWARE = "$profile:OpenZone\\Hardware.json";

    static const int SCHEMA_PROFILES = 1;

    // Імена слотів. Батарея -- ванільна: рушій сам втикає пристрій у неї.
    // Решта наші, і рушій про них не знає нічого -- що в них влазить і що це
    // дає, вирішує таблиця класнеймів у Hardware.json.
    static const string SLOT_BATTERY = "BatteryD";
    static const string SLOT_ANTENNA = "OZ_Antenna";
    static const string SLOT_CARRIER = "OZ_DataCarrier";

    // Id меню для EnterScriptedMenu.
    //
    // Мусить бути > 46 (останній ванільний у constants.c), але й НЕ завеликим:
    // рушій відкидає завеликий id ще ДО того, як спитати місію -- метод
    // EnterScriptedMenu просто повертає NULL, а Mission.CreateScriptedMenu
    // навіть не викликається. Сусідній мод спіймав це діагностикою: у лозі є
    // виклики з ванільними 11 і 17, а з шестизначним -- жодного.
    //
    // Реєстру id у рушії немає, тож зіткнення з ЧУЖИМ модом можливе й
    // непереборне. Єдиний захист -- перевіряти FindMenu перед відкриттям.
    static const int MENU_PDA = 131;

    // Ім'я інпута з data/inputs.xml. Імена глобальні для ВСІХ завантажених
    // модів, тому префікс обов'язковий.
    static const string INPUT_OPEN = "UAOZPdaOpen";
    static const string INPUT_PTT  = "UAOZPdaPtt";

    // Сторінка, яку несе сам КПК.
    static const string PAGE_DEVICE = "device";
}
