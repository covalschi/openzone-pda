// Штампи часу ЛОКАЛЬНИМ годинником клієнта.
//
// Міст пише час у UTC, і чесніше за все показати його так, як показує
// телефон гравця: рушій дає обидва годинники (ensystem.c), різниця між
// ними і є зсув пояса цієї машини. Лютий тут завжди 28: помилка на добу
// в штампі чату раз на чотири роки не варта таблиці високосності.
class OZ_LocalTime
{
    static int OffsetMin()
    {
        int lh;
        int lm;
        int ls;
        GetHourMinuteSecond(lh, lm, ls);

        int uh;
        int um;
        int us;
        GetHourMinuteSecondUTC(uh, um, us);

        int diff = (lh * 60 + lm) - (uh * 60 + um);
        if (diff > 720)
            diff -= 1440;
        if (diff < -720)
            diff += 1440;
        return diff;
    }

    private static int DaysIn(int mo)
    {
        if (mo == 4 || mo == 6 || mo == 9 || mo == 11)
            return 30;
        if (mo == 2)
            return 28;
        return 31;
    }

    private static string Two(int v)
    {
        if (v < 10)
            return "0" + v.ToString();
        return v.ToString();
    }

    // "YYYY-MM-DD HH:MM..." (UTC, T чи пробіл -- байдуже: зрізи позиційні)
    // -> "DD.MM  HH:MM" локального часу.
    static string Stamp(string iso)
    {
        if (iso.Length() < 16)
            return iso;

        int mo = iso.Substring(5, 2).ToInt();
        int d  = iso.Substring(8, 2).ToInt();
        int h  = iso.Substring(11, 2).ToInt();
        int mi = iso.Substring(14, 2).ToInt();

        int tot = h * 60 + mi + OffsetMin();
        if (tot >= 1440)
        {
            tot -= 1440;
            d++;
        }
        else if (tot < 0)
        {
            tot += 1440;
            d--;
        }

        if (d < 1)
        {
            mo--;
            if (mo < 1)
                mo = 12;
            d = DaysIn(mo);
        }
        else if (d > DaysIn(mo))
        {
            d = 1;
            mo++;
            if (mo > 12)
                mo = 1;
        }

        return Two(d) + "." + Two(mo) + "  " + Two(tot / 60) + ":" + Two(tot % 60);
    }
}
