// Шлюз до меню для нижніх шарів.
//
// Порядок компіляції жорсткий: 3_Game -> 4_World -> 5_Mission, і знизу вгору
// видимості немає. Дія «відкрити КПК» живе в 4_World, а саме меню -- у
// 5_Mission, тож напряму покликати його вона не може.
//
// Розв'язка -- база тут і нащадок там: місія створює свого нащадка й кладе
// сюди, після чого будь-який шар зве OZ_PdaMenuGate.Open() і не знає нічого
// про UI. На виділеному сервері нащадка не існує, Open() тихо нічого не
// робить -- саме те, що треба.

class OZ_PdaMenuGate
{
    private static ref OZ_PdaMenuGate s_Inst;

    static void Bind(OZ_PdaMenuGate inst)
    {
        s_Inst = inst;
    }

    static void Open()
    {
        if (s_Inst)
            s_Inst.DoOpen();
    }

    static void Close()
    {
        if (s_Inst)
            s_Inst.DoClose();
    }

    static bool IsBound()
    {
        return s_Inst != null;
    }

    // ---- «КПК без предмета» (D132) ----
    //
    // Меню одне, а приводів відкрити його два: дія на приладі й дозвіл
    // адміна без приладу. Прапорець живе тут, бо сюди дивляться обидва
    // кінці: місія ставить його, отримавши "pda_virtual" від сервера, а меню
    // читає, вирішуючи, чи закриватись через порожні руки. Знімає його саме
    // закриття (OnHide), щоб наступне відкриття дією не успадкувало чужий
    // режим.
    private static bool s_Virtual = false;

    static void OpenVirtual()
    {
        s_Virtual = true;
        Open();
    }

    static bool Virtual()
    {
        return s_Virtual;
    }

    static void ClearVirtual()
    {
        s_Virtual = false;
    }

    // Перевизначає нащадок у 5_Mission.
    void DoOpen()  { }
    void DoClose() { }
}
