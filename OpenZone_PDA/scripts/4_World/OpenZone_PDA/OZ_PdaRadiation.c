// Договір на радіацію.
//
// КПК НЕ вигадує цифр. Радіометр і дозиметр -- це екрани, а числа дає мод
// радіації: WTHealth із @Radio, TerjeRadiation, або будь-який свій. Той самий
// підхід, що й із журналом завдань: пристрій везе прилад і форму даних,
// постачальник везе значення.
//
// Різниця між двома приладами -- фізична, і плутати їх не можна:
//
//   РАДІОМЕТР (лічильник Гейгера) міряє ЗОВНІШНЄ поле тут і зараз. Показує,
//   наскільки небезпечно СТОЯТИ в цьому місці. Мікрозіверти на годину.
//
//   ДОЗИМЕТР (ін'єкційний) міряє НАКОПИЧЕНУ дозу в тілі. Показує, скільки ти
//   вже отримав за весь час. Мікрозіверти. Він не падає, коли ти вийшов із
//   плями -- у цьому й сенс.
//
// Без постачальника обидва чесно кажуть «немає даних», а не малюють нулі:
// нуль означає «чисто», і брехати цим не можна.

class OZ_RadiationReading
{
    bool  HasProvider = false;
    string ProviderName = "";

    // Зовнішнє поле в точці гравця, мкЗв/год. Від'ємне = немає даних.
    float AmbientUSvH = -1;

    // Накопичена доза, мкЗв. Від'ємне = немає даних.
    float DoseUSv = -1;

    // Поріг, за яким постачальник вважає дозу небезпечною. Дає приладу
    // право фарбувати шкалу, не знаючи чужої моделі здоров'я.
    float DoseWarnUSv = -1;
}

class OZ_RadiationProvider
{
    string Name()
    {
        return "unknown";
    }

    // Зовнішнє поле в точці. Повернути від'ємне, якщо не вміє.
    float AmbientUSvH(vector pos)
    {
        return -1;
    }

    // Накопичена доза гравця. Повернути від'ємне, якщо не вміє.
    float DoseUSv(PlayerBase player)
    {
        return -1;
    }

    float DoseWarnUSv()
    {
        return -1;
    }
}

class OZ_PdaRadiation
{
    private static ref OZ_RadiationProvider s_Provider;

    static void Bind(OZ_RadiationProvider provider)
    {
        if (s_Provider)
        {
            string w = "radiation provider replaced: " + s_Provider.Name();
            w += " -> " + provider.Name();
            OZ_Log.Warn(w);
        }
        s_Provider = provider;
        OZ_Log.Info("radiation provider: " + provider.Name());
    }

    static bool HasProvider()
    {
        return s_Provider != null;
    }

    // wantAmbient / wantDose -- які прилади реально вставлені в пристрій.
    // Питати те, чого нема чим міряти, безглуздо.
    static OZ_RadiationReading Read(PlayerBase player, bool wantAmbient, bool wantDose)
    {
        OZ_RadiationReading r = new OZ_RadiationReading();

        if (!s_Provider || !player)
            return r;

        r.HasProvider  = true;
        r.ProviderName = s_Provider.Name();

        if (wantAmbient)
            r.AmbientUSvH = s_Provider.AmbientUSvH(player.GetPosition());

        if (wantDose)
        {
            r.DoseUSv     = s_Provider.DoseUSv(player);
            r.DoseWarnUSv = s_Provider.DoseWarnUSv();
        }

        return r;
    }
}
