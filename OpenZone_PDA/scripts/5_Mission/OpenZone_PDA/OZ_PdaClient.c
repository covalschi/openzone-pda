// Клієнтські дрібниці про власний пристрій.
//
// Клієнт НЕ вирішує нічого про КПК -- усе питає в сервера. Але для прев'ю та
// для звуків йому потрібна сама сутність предмета, і питати її по проводу
// було б безглуздо: вона в нього вже є, треба лише знати, котра саме.

class OZ_PdaClient
{
    // Пристрій, про який відповів сервер.
    //
    // Шукати самотужки клієнт не має права: сервер бере руки, а якщо там
    // порожньо -- обшукує інвентар. Клієнт, який дивиться лише в руки,
    // покаже порожнє прев'ю там, де сервер цілком певно говорить про КПК у
    // рюкзаку. Саме це й було видно на живому клієнті.
    static EntityAI Device(OZ_PdaDeviceStatus st)
    {
        if (!st)
            return null;

        // NetLow і NetHigh нулями одночасно бути не можуть у справжньої
        // мережевої сутності -- це ознака старої відповіді без адреси.
        if (st.NetLow == 0 && st.NetHigh == 0)
            return HeldEntity();

        Object obj = GetGame().GetObjectByNetworkId(st.NetLow, st.NetHigh);
        if (!obj)
        {
            // Сутність ще не приїхала клієнту або вже вивантажена. Це не
            // помилка: наступний device/status спробує знову.
            return HeldEntity();
        }

        return EntityAI.Cast(obj);
    }

    // Предмет у руках, якщо це КПК. Потрібен звукам і запасним шляхом для
    // прев'ю: звук пристрою має йти від того, що гравець тримає.
    static EntityAI HeldEntity()
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p)
            return null;

        OZ_PDA_Base pda = OZ_PDA_Base.Cast(p.GetItemInHands());
        if (!pda)
            return null;

        return pda;
    }

    static bool HasPdaInHands()
    {
        return HeldEntity() != null;
    }
}
