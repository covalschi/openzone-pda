// Довгі рядки в сховищі предмета -- ШМАТКАМИ.
//
// Виміряно зондом на стенді 2026-08-28: рядок у сховищі сутності живе
// лише до 1023 байтів. 1024-й байт -- і завантаження кидає
// "String CORRUPTED - FIX OnStoreLoad()", причому гине не рядок, а ВЕСЬ
// мод-блоб предмета: пін, мітки, вміст чипа -- усе разом (сам предмет
// виживає порожнім). Той самий 1-КіБ буфер ріже значення в JsonFileLoader
// (1023) і псував наш RPC на ~1024 -- це одна й та сама межа рушія.
//
// Тому будь-який рядок, що МОЖЕ перерости 1023 (JSON міток, пейлоад
// носія), їде шматками <= 1000 байтів: int-лічильник, потім рядки.
// Збирання -- конкатенацією, вона безстельова (зміряно до 1 МіБ).
class OZ_StoreBig
{
    static const int PIECE = 1000;

    static void Write(CF_ModStorage ctx, string s)
    {
        int len = s.Length();

        int pieces = 0;
        if (len > 0)
            pieces = (len + PIECE - 1) / PIECE;

        ctx.Write(pieces);

        int off = 0;
        while (off < len)
        {
            int take = PIECE;
            if (len - off < take)
                take = len - off;

            // Вихід Substring ріжеться до 8191 -- шматок на 1000 нижче.
            ctx.Write(s.Substring(off, take));
            off += take;
        }
    }

    static bool Read(CF_ModStorage ctx, out string s)
    {
        s = "";

        // СУМІСНІСТЬ: старі збереження тримають на цій позиції РЯДОК, а не
        // лічильник. Записи CF v5 типізовані, і невдалий Read(int) курсор
        // НЕ рухає -- тож питаємо про новий формат, а на відмові чесно
        // читаємо старий одним рядком.
        int pieces;
        if (!ctx.Read(pieces))
            return ctx.Read(s);

        for (int i = 0; i < pieces; i++)
        {
            string part;
            if (!ctx.Read(part))
                return false;
            s += part;
        }

        return true;
    }
}
