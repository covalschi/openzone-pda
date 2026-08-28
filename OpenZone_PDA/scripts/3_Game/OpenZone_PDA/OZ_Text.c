// Обрізання рядка по БАЙТАХ без розрізання символу навпіл.
//
// string.Length() і Substring() в Enforce байтові, а текст у полях -- UTF-8:
// кирилична літера займає два байти, і сліпий Substring(0, max) інколи
// лишає від неї перший байт без другого. Такий хвіст не прочитає жоден
// декодер -- Discord малює замість нього U+FFFD. Зміряно на
// «Схрон біля Доброго»: 33 байти різались у 32 між літерами лише випадково.
class OZ_Text
{
    static string Clip(string s, int maxBytes)
    {
        if (s.Length() <= maxBytes)
            return s;

        int cut = maxBytes;
        // Байт-продовження UTF-8 має вигляд 10xxxxxx. Відступаємо до
        // початку символу: все, що лівіше, -- лише цілі літери.
        while (cut > 0 && (s.Get(cut).ToAscii() & 0xC0) == 0x80)
            cut--;

        return s.Substring(0, cut);
    }
}
