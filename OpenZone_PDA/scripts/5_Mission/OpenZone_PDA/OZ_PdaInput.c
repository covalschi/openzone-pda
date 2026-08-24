// Опитування клавіші відкриття.
//
// Модовий інпут НЕ має числового id -- рушій роздає їх лише своїм ~150
// вбудованим. Тому GetInputByID тут не працює взагалі, тільки GetInputByName.
//
// Сам UAInput кешувати НЕ можна: ваніль тримає обгортку й перечитує вказівник
// щокадру (radialmenu.c, actiontargets). Робимо так само.

class OZ_PdaInput
{
    // БЕЗ ref: UAIDWrapper -- нативний об єкт із приватним деструктором,
    // і скрипт ним не володіє. Ваніль тримає його так само (radialmenu.c:31).
    private static UAIDWrapper s_Open;
    private static bool s_Warned = false;

    static void Init()
    {
        UAInput i = GetUApi().GetInputByName(OZ_PdaConst.INPUT_OPEN);
        if (!i)
        {
            // Єдина діагностика, яку дає рушій: NULL. Причин рівно дві --
            // inputs.xml не завантажився (шлях у CfgMods) або ім'я написане
            // інакше, ніж у XML.
            if (!s_Warned)
            {
                s_Warned = true;
                OZ_Log.Error("input " + OZ_PdaConst.INPUT_OPEN + " not found - check the CfgMods inputs= path and the name in inputs.xml");
            }
            return;
        }

        s_Open = i.GetPersistentWrapper();
        OZ_Log.Dbg("input " + OZ_PdaConst.INPUT_OPEN + " bound");
    }

    static void Poll()
    {
        if (!s_Open)
            return;

        UAInput i = s_Open.InputP();
        if (!i || !i.LocalPress())
            return;

        // Друге натискання закриває -- клавіша поводиться як вимикач, а не як
        // кнопка «відкрити ще раз».
        if (GetGame().GetUIManager().FindMenu(OZ_PdaConst.MENU_PDA))
        {
            OZ_PdaMenuGate.Close();
            return;
        }

        OZ_PdaMenuGate.Open();
    }
}
