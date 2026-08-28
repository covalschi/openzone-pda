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
    private TextListboxWidget m_List;
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
        m_List      = TextListboxWidget.Cast(Wgt("NoteList"));
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

    override bool OnPageItemSelected(Widget w, int row)
    {
        if (!m_List || w != m_List)
            return false;

        OpenSelected(row);
        return true;
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
        if (m_List)
            m_List.ClearItems();

        if (!m_Book.Notes)
            return;

        for (int i = 0; i < m_Book.Notes.Count(); i++)
        {
            OZ_Note n = m_Book.Notes[i];

            string row = n.Title;
            if (row == "")
                row = "#STR_OZ_NOTES_UNTITLED";

            if (m_List)
            {
                // Дата -- ОКРЕМА колонка: довга назва обрізається своєю
                // шириною й дати не чіпає. "12.08" вистачає, щоб упізнати свіже.
                string when = "";
                if (n.EditedAt.Length() >= 10)
                    when = n.EditedAt.Substring(8, 2) + "." + n.EditedAt.Substring(5, 2);

                int at = m_List.AddItem(row, NULL, 0);
                m_List.SetItem(at, when, NULL, 1);
            }
        }

        if (m_Book.Notes.Count() == 0 && !m_Draft)
            SetText("NotesHint", "#STR_OZ_NOTES_NONE");
    }
}
