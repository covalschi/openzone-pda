// Поведінка модуля -- те, чим чужий мод дає КПК здатність, якої в ньому
// спочатку не було.
//
// Таблиця Hardware.json описує модуль ДАНИМИ: класнейм, вид, витрата, які
// сторінки вмикає. Цього досить для антени чи носія, але не для приладу, який
// щось РОБИТЬ -- детектор аномалій має пищати, коли поруч аномалія, і жодна
// таблиця цього не висловить.
//
// Тому модуль може принести ще й КОД. Мод оголошує нащадка цього класу,
// кладе його через OZ_PdaModules.Register(), і КПК починає його кликати.
// Сам КПК про аномалії, радіацію й будь-що інше не знає нічого й знати не
// повинен.
//
// НАВАНТАЖЕННЯ. OnTick кличеться не щокадру, а раз на TickSeconds -- і це не
// порада, а вимога. Детектор, що опитує світ кожен кадр на кожному КПК
// сервера, і є той самий генератор лаг-спайків, про який ідеться в спеці.
// Модуль сам називає свій період: детектору вистачає раз на пів секунди,
// дозиметру -- раз на десять.

class OZ_ModuleBehaviour
{
    // Вид, до якого прив'язана ця поведінка. Збігається з Kind у Hardware.json.
    string Kind()
    {
        return "";
    }

    // Ім'я мода-власника, для лога.
    string Owner()
    {
        return "unknown";
    }

    // Як часто кликати OnTick. Нуль або менше -- не кликати взагалі: модуль
    // суто декларативний, як антена.
    float TickSeconds()
    {
        return 0;
    }

    // Модуль вставили. Серверно.
    void OnAttached(ItemBase pda, int slotIndex)
    {
    }

    // Модуль вийняли. Серверно. Тут гасять звуки й ефекти, які модуль завів:
    // КПК за чужим модулем не прибирає.
    void OnDetached(ItemBase pda, int slotIndex)
    {
    }

    // Періодична робота, поки пристрій УВІМКНЕНИЙ і модуль вставлений.
    //
    // Кличеться і на сервері, і на клієнті -- звук аномалії чути мусить сам
    // гравець, а це клієнтська справа; а от рішення, чи є аномалія поруч,
    // може бути й серверним. Розбирається сам модуль через GetGame().IsServer().
    void OnTick(ItemBase pda, Man owner, float deltaSeconds)
    {
    }
}

class OZ_PdaModules
{
    private static ref map<string, ref OZ_ModuleBehaviour> s_Behaviours;

    private static void Ensure()
    {
        if (!s_Behaviours)
            s_Behaviours = new map<string, ref OZ_ModuleBehaviour>();
    }

    // Поведінка одна на ВИД, не на класнейм: п'ять різних антен від різних
    // модів поводяться однаково, і змушувати кожного реєструватись окремо
    // означало б плодити копії того самого коду.
    static void Register(OZ_ModuleBehaviour b)
    {
        Ensure();

        string kind = b.Kind();
        if (kind == "")
        {
            OZ_Log.Warn("module behaviour from " + b.Owner() + " has no Kind - ignored");
            return;
        }

        if (s_Behaviours.Contains(kind))
        {
            string w = "module behaviour for \"" + kind;
            w += "\" replaced: " + s_Behaviours.Get(kind).Owner();
            w += " -> " + b.Owner();
            OZ_Log.Warn(w);
        }

        s_Behaviours.Set(kind, b);

        string msg = "module behaviour registered: " + kind;
        msg += " by " + b.Owner();
        OZ_Log.Info(msg);
    }

    static OZ_ModuleBehaviour For(string kind)
    {
        Ensure();
        if (!s_Behaviours.Contains(kind))
            return null;
        return s_Behaviours.Get(kind);
    }

    // Поведінка модуля, що лежить у відсіку: класнейм -> вид -> поведінка.
    static OZ_ModuleBehaviour ForClass(string cls)
    {
        OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
        if (!spec)
            return null;
        return For(spec.Kind);
    }

    static int Count()
    {
        Ensure();
        return s_Behaviours.Count();
    }
}
