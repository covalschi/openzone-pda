// Сторінка «Записки»: перелік ліворуч, редактор праворуч.
//
// Текст набирається у ВЛАСНИХ віджетах вводу (EditBoxWidget і
// MultilineEditBoxWidget), а не через OnKeyPress меню. Причина виміряна на
// живому клієнті: до OnKeyPress ввід доходить не з усякого джерела, а поля
// вводу ловлять клавіатуру самі -- цим же віджетом ваниль дає писати на
// папері.
//
// Зберігає ЛИШЕ кнопка. Список навмисно не перечитується сам собою: інакше
// чергове оновлення затирало б те, що гравець набирає просто зараз.

class OZ_PdaPageNotes : OZ_PdaPage
{
    private Widget m_Canvas;
    private ref array<Widget> m_NoteRows = new array<Widget>();
    private int m_NotesY = 0;
    private EditBoxWidget m_Title;
    private MultilineEditBoxWidget m_Body;
    private ButtonWidget m_BtnNew;
    private ButtonWidget m_BtnSave;
    private ButtonWidget m_BtnDelete;
    private ButtonWidget m_BtnToCar;

    private ref OZ_NoteBook m_Book;
    private string m_CurrentId = "";
    private bool m_Draft = false;   // відкрито НОВУ, ще не збережену
    // Збереження в польоті: відповідь відкладена (міст), і другий клік по
    // SAVE до її приходу створював ДРУГУ записку -- чернетка шле Id="",
    // а «створи» без Id міст виконує щоразу.
    private bool m_Saving = false;

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_notes.layout";
    }

    override void OnBuilt()
    {
        m_Canvas    = Wgt("NotesCanvas");
        m_Title     = EditBoxWidget.Cast(Wgt("NoteTitle"));
        m_Body      = MultilineEditBoxWidget.Cast(Wgt("NoteBody"));
        m_BtnNew    = ButtonWidget.Cast(Wgt("BtnNew"));
        m_BtnSave   = ButtonWidget.Cast(Wgt("BtnSave"));
        m_BtnDelete = ButtonWidget.Cast(Wgt("BtnDelete"));
        m_BtnToCar = ButtonWidget.Cast(Wgt("BtnNoteToCar"));

        SetText("BtnNewText", "#STR_OZ_NOTES_NEW");
        SetText("BtnSaveText", "#STR_OZ_NOTES_SAVE");
        SetText("BtnDeleteText", "#STR_OZ_NOTES_DELETE");
        SetText("BtnNoteToCarText", "#STR_OZ_TO_CARRIER");
    }

    override void OnSelected()
    {
        m_Saving = false;
        // Рушій сам дає фокус першому multiline-полю при побудові, і
        // порожнє тіло світилось червоним фокус-стилем на всю панель.
        SetFocus(null);

        Request();
    }

    // Записки не змінюються самі: перечитувати їх щосекунди означало б
    // затирати те, що гравець ЗАРАЗ набирає. Тому оновлення тільки за подією.
    override void OnRefresh()
    {
    }

    private void Request()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_NOTES, "list", "{}");
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (w && w.GetUserID() == 7)
        {
            OpenSelected(w.GetName().ToInt());
            PaintPicks();
            return true;
        }

        if (!w)
            return false;

        if (w == m_BtnNew)
        {
            m_CurrentId = "";
            m_Draft     = true;
            if (m_Title)
                m_Title.SetText("");
            if (m_Body)
                m_Body.SetText("");
            SetText("NotesHint", "#STR_OZ_NOTES_DRAFT");
            return true;
        }

        if (w == m_BtnSave)
        {
            SendSave();
            return true;
        }

        if (w == m_BtnToCar)
        {
            // Експорт ОБРАНОЇ записки в тому вигляді, як вона ЗБЕРЕЖЕНА:
            // чернетка та незбережені правки на чип не їдуть.
            if (m_CurrentId == "" || m_Draft || !m_Book || !m_Book.Notes)
                return true;

            OZ_Note picked;
            for (int ci = 0; ci < m_Book.Notes.Count(); ci++)
            {
                if (m_Book.Notes[ci].Id == m_CurrentId)
                {
                    picked = m_Book.Notes[ci];
                    break;
                }
            }

            if (!picked)
                return true;

            string cjson;
            string cerr;
            if (JsonFileLoader<OZ_Note>.MakeData(picked, cjson, cerr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_NOTES, "carrier_add", cjson);
            return true;
        }

        if (w == m_BtnDelete)
        {
            if (m_CurrentId == "")
            {
                // Нову й ще не збережену видаляти нема чого -- просто кидаємо
                // чернетку. Мовчазна відмова тут виглядала б як поломка.
                m_Draft = false;
                if (m_Title)
                    m_Title.SetText("");
                if (m_Body)
                    m_Body.SetText("");
                SetText("NotesHint", "");
                return true;
            }

            OZ_NoteRef r = new OZ_NoteRef();
            r.Id = m_CurrentId;

            string json;
            string err;
            if (JsonFileLoader<OZ_NoteRef>.MakeData(r, json, err, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_NOTES, "delete", json);
            return true;
        }

        return false;
    }


    private void OpenSelected(int row)
    {
        if (!m_Book || !m_Book.Notes)
            return;
        if (row < 0 || row >= m_Book.Notes.Count())
            return;

        OZ_Note n = m_Book.Notes[row];
        m_CurrentId = n.Id;
        m_Draft     = false;

        if (m_Title)
            m_Title.SetText(n.Title);
        if (m_Body)
            m_Body.SetText(n.Body);

        string when = "#STR_OZ_NOTES_EDITED";
        when += "  " + n.EditedAt;
        SetText("NotesHint", when);
    }

    private void SendSave()
    {
        if (m_Saving)
            return;

        OZ_Note n = new OZ_Note();
        n.Id = m_CurrentId;

        if (m_Title)
            n.Title = m_Title.GetText();

        if (m_Body)
        {
            string body;
            // Не сирий GetText: він віддає ВІЗУАЛЬНІ переноси як \n, і
            // збережений текст обростав розривами посеред слів ("the b/arn").
            // OZ_Unwrap лишає тільки Enter-и людини.
            body = OZ_Unwrap.Read(m_Body, TextWidget.Cast(Wgt("WrapRuler")));
            n.Body = body;
        }

        if (n.Title == "" && n.Body == "")
        {
            SetText("NotesHint", "#STR_OZ_NOTES_EMPTY");
            return;
        }

        string json;
        string err;
        if (!JsonFileLoader<OZ_Note>.MakeData(n, json, err, false))
            return;

        m_Saving = true;
        OZ_Rpc.Request(OZ_PdaConst.PAGE_NOTES, "save", json);
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (op == "carrier_add")
        {
            if (ok)
                SetHintSticky("NotesHint", "#STR_OZ_CARRIER_SAVED");
            else
                SetHintSticky("NotesHint", "#" + error);
            return;
        }

        if (op == "save" || op == "delete")
        {
            if (op == "save")
                m_Saving = false;

            if (!ok)
            {
                SetText("NotesHint", "#" + error);
                return;
            }

            // Видалили -- редактор порожній.
            if (op == "delete")
            {
                m_CurrentId = "";
                m_Draft     = false;
                if (m_Title)
                    m_Title.SetText("");
                if (m_Body)
                    m_Body.SetText("");

                SetText("NotesHint", "#STR_OZ_NOTES_DELETED");
                Request();
                return;
            }

            // Зберегли -- редактор лишається відкритим, але тепер він знає,
            // ЩО саме відкрито. Сервер повертає id збереженої записки, і поки
            // він сюди не доїхав, друге натискання «Зберегти» слало порожній
            // id, тобто просило створити ще одну. Чернетка, збережена двічі,
            // ставала двома записками.
            //
            // Ветка редагування теж проходить тут: id той самий, присвоєння
            // безпечне, а чернеткою запис перестає бути в обох випадках.
            OZ_NoteRef saved;
            string refErr;
            if (JsonFileLoader<OZ_NoteRef>.LoadData(json, saved, refErr) && saved && saved.Id != "")
                m_CurrentId = saved.Id;

            m_Draft = false;

            SetText("NotesHint", "#STR_OZ_NOTES_SAVED");
            Request();
            return;
        }

        if (op != "list")
            return;

        if (!ok)
        {
            SetText("NotesHint", "#" + error);
            return;
        }

        string err;
        OZ_NoteBook b;
        if (!JsonFileLoader<OZ_NoteBook>.LoadData(json, b, err))
        {
            OZ_Log.Error("notes unreadable: " + err);
            return;
        }

        m_Book = b;
        Paint();
    }

    private void Paint()
    {
        for (int r = 0; r < m_NoteRows.Count(); r++)
        {
            if (m_NoteRows[r])
                m_NoteRows[r].Unlink();
        }
        m_NoteRows.Clear();
        m_NotesY = 0;

        if (!m_Book.Notes)
            return;

        for (int i = 0; i < m_Book.Notes.Count(); i++)
        {
            OZ_Note n = m_Book.Notes[i];

            string title = n.Title;
            if (title == "")
                title = "#STR_OZ_NOTES_UNTITLED";

            Widget w = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_note_row.layout", m_Canvas);
            if (!w)
                continue;

            w.SetUserID(7);
            w.SetName(i.ToString());
            w.SetPos(0, m_NotesY);
            m_NotesY += 26;

            TextWidget tw = TextWidget.Cast(w.FindAnyWidget("RowTitle"));
            if (tw)
                tw.SetText(title);

            // Дата -- окремим стовпчиком праворуч: довга назва обрізається
            // своєю шириною й дати не чіпає.
            TextWidget dw = TextWidget.Cast(w.FindAnyWidget("RowDate"));
            if (dw && n.EditedAt.Length() >= 10)
                dw.SetText(OZ_LocalTime.Stamp(n.EditedAt).Substring(0, 5));

            m_NoteRows.Insert(w);
        }

        if (m_Canvas)
        {
            int want = m_NotesY;
            if (want < 356)
                want = 356;
            m_Canvas.SetSize(340, want);
        }

        PaintPicks();

        if (m_Book.Notes.Count() == 0 && !m_Draft)
            SetText("NotesHint", "#STR_OZ_NOTES_NONE");
    }

    // Смужка вибору на рядку відкритої записки.
    private void PaintPicks()
    {
        if (!m_Book || !m_Book.Notes)
            return;

        for (int i = 0; i < m_NoteRows.Count(); i++)
        {
            if (!m_NoteRows[i])
                continue;

            Widget pick = m_NoteRows[i].FindAnyWidget("RowPick");
            if (pick && i < m_Book.Notes.Count())
                pick.Show(m_Book.Notes[i].Id == m_CurrentId);
        }
    }
}
