// Клієнтські дрібниці про власний пристрій.
//
// Клієнт НЕ вирішує нічого про КПК -- усе питає в сервера. Але для прев'ю та
// для звуків йому потрібна сама сутність предмета в руках, і питати її по
// проводу було б безглуздо: вона в нього вже є.

class OZ_PdaClient
{
    // Предмет у руках, якщо це КПК. Інвентар НЕ обшукуємо: прев'ю показує
    // те, що гравець тримає, а не те, що десь у рюкзаку.
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
