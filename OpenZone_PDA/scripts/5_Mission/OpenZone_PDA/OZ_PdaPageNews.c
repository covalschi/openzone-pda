// Сторінка «Новини»: список постів зліва, тіло обраного справа.
//
// Читалка для всіх -- і, з 2026-09-02, перо для лідера (ТЗ-6 R2.1). Кнопка
// WRITE з'являється лише тому, кому міст відповів "voices" з Leader або
// Admin; список імен для підпису -- теж від моста. Сторінка не вирішує
// прав і не вигадує імен (R2.3): вона малює те, що їй віддали, а маршрут
// запису перевіряє ще раз (R3.2).

class OZ_PdaPageNews : OZ_PdaPage
{
    private Widget m_Rows;
    private ref array<Widget> m_RowWgts;
    private ref OZ_NewsList m_List;
    private string m_OpenId = "";

    // Перо лідера.
    private Widget m_Compose;
    private Widget m_BtnWrite;
    private ref array<string> m_Voices;
    private string m_Self = "";
    private int    m_Pick = 0;

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_news.layout";
    }

    override void OnBuilt()
    {
        m_Rows    = Wgt("NewsRows");
        m_RowWgts = new array<Widget>();
        m_Voices  = new array<string>();

        m_Compose  = Wgt("ComposePanel");
        m_BtnWrite = Wgt("BtnWrite");
        if (m_Compose)
            m_Compose.Show(false);
        if (m_BtnWrite)
            m_BtnWrite.Show(false);

        SetText("BtnWriteText",     "#STR_OZ_NEWS_WRITE");
        SetText("BtnCmpSendText",   "#STR_OZ_NEWS_SEND");
        SetText("BtnCmpCancelText", "#STR_OZ_NEWS_CANCEL");
        SetText("CmpHead",          "#STR_OZ_NEWS_COMPOSE");
    }

    override void OnSelected()
    {
        ClearHintHold();
        OZ_Rpc.Request(OZ_PdaConst.PAGE_NEWS, "list", "{}");

        // Хто я для новин -- питаємо щоразу: грант могли зняти, поки сторінка
        // була закрита (приймання 5.9), і кнопка мусить зникнути разом із ним.
        OZ_Rpc.Request(OZ_PdaConst.PAGE_NEWS, "voices", "{}");
    }

    override void OnDeselected()
    {
        super.OnDeselected();
        if (m_Compose)
            m_Compose.Show(false);
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        string nm = w.GetName();

        if (nm == "BtnWrite")
        {
            if (m_Compose)
                m_Compose.Show(true);
            m_Pick = 0;
            PaintWho();
            return true;
        }

        if (nm == "BtnCmpCancel")
        {
            if (m_Compose)
                m_Compose.Show(false);
            return true;
        }

        if (nm == "BtnCmpWho")
        {
            m_Pick++;
            if (m_Pick > m_Voices.Count())
                m_Pick = 0;
            PaintWho();
            return true;
        }

        if (nm == "BtnCmpSend")
        {
            Send();
            return true;
        }

        // Рядок поста. Ім'я віджета -- Id треда форуму.
        if (w.GetUserID() == 7)
        {
            m_OpenId = w.GetName();

            OZ_NewsRef r = new OZ_NewsRef();
            r.Id = m_OpenId;

            string json;
            string err;
            if (JsonFileLoader<OZ_NewsRef>.MakeData(r, json, err, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_NEWS, "open", json);

            Repaint();
            return true;
        }

        return false;
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (!ok)
        {
            // Без відповіді про імена пера не буде -- і без окремого докору:
            // причину (міст лежить) уже сказав той самий відказ на "list".
            if (op == "voices")
            {
                if (m_BtnWrite)
                    m_BtnWrite.Show(false);
                return;
            }
            SetHintSticky("NewsHint", "#" + error);
            return;
        }

        string err;

        if (op == "voices")
        {
            // Не 'v': так зветься OZ_NewsView у гілці open нижче, а Enforce не
            // дає оголосити одне ім'я двічі в сусідніх гілках однієї функції.
            OZ_NewsVoices vo;
            if (!JsonFileLoader<OZ_NewsVoices>.LoadData(json, vo, err) || !vo)
                return;

            m_Self = vo.Self;
            m_Voices.Clear();
            if (vo.Voices)
            {
                for (int k = 0; k < vo.Voices.Count(); k++)
                    m_Voices.Insert(vo.Voices[k]);
            }
            m_Pick = 0;

            bool canWrite = vo.Leader || vo.Admin;
            if (m_BtnWrite)
                m_BtnWrite.Show(canWrite);
            if (!canWrite && m_Compose)
                m_Compose.Show(false);
            PaintWho();
            return;
        }

        if (op == "post")
        {
            if (m_Compose)
                m_Compose.Show(false);

            EditBoxWidget te = EditBoxWidget.Cast(Wgt("CmpTitle"));
            if (te)
                te.SetText("");
            MultilineEditBoxWidget be = MultilineEditBoxWidget.Cast(Wgt("CmpBody"));
            if (be)
                be.SetText("");

            SetHint("NewsHint", "#STR_OZ_NEWS_POSTED");
            OZ_Rpc.Request(OZ_PdaConst.PAGE_NEWS, "list", "{}");
            return;
        }

        if (op == "push")
        {
            // Свіжий пост -- перечитуємо перелік, поки сторінка відкрита.
            OZ_Rpc.Request(OZ_PdaConst.PAGE_NEWS, "list", "{}");
            return;
        }

        if (op == "list")
        {
            OZ_NewsList l;
            if (!JsonFileLoader<OZ_NewsList>.LoadData(json, l, err) || !l)
                return;

            m_List = l;
            Repaint();
            return;
        }

        if (op == "open")
        {
            OZ_NewsView v;
            if (!JsonFileLoader<OZ_NewsView>.LoadData(json, v, err) || !v)
                return;

            SetText("PostTitle", v.Title);
            SetText("PostMeta", v.Who + "   " + Day(v.At));

            MultilineTextWidget body = MultilineTextWidget.Cast(Wgt("PostBody"));
            if (body)
                body.SetText(v.Body);

            // The spacer measures itself only on Update(): a new post's text
            // keeps the old height until the next relayout without it.
            Widget ps = Wgt("PostStack");
            if (ps)
                ps.Update();

            SetHint("NewsHint", "");
            return;
        }
    }

    // Кнопка підпису показує вибір: нуль -- своє ім'я, далі персони ГП.
    private void PaintWho()
    {
        string label;
        if (m_Pick <= 0 || m_Pick > m_Voices.Count())
        {
            m_Pick = 0;
            label = Widget.TranslateString("#STR_OZ_NEWS_AS_SELF");
            if (m_Self != "")
                label = m_Self + "  (" + label + ")";
        }
        else
        {
            label = m_Voices[m_Pick - 1];
        }
        SetText("BtnCmpWhoText", label);
    }

    private string PickedVoice()
    {
        if (m_Pick <= 0 || m_Pick > m_Voices.Count())
            return "";
        return m_Voices[m_Pick - 1];
    }

    private void Send()
    {
        string title = "";
        string body  = "";

        EditBoxWidget te = EditBoxWidget.Cast(Wgt("CmpTitle"));
        if (te)
            title = te.GetText();
        MultilineEditBoxWidget be = MultilineEditBoxWidget.Cast(Wgt("CmpBody"));
        if (be)
            be.GetText(body);

        // Порожнє відхиляємо на місці: сервер відповів би тим самим, але за
        // круг, і гравець чекав би на те, що бачить сам.
        if (title.Trim() == "")
        {
            SetHintSticky("NewsHint", "#STR_OZ_ERR_NEWS_NO_TITLE");
            return;
        }
        if (body.Trim() == "")
        {
            SetHintSticky("NewsHint", "#STR_OZ_ERR_NEWS_NO_BODY");
            return;
        }

        OZ_NewsPostAsk p = new OZ_NewsPostAsk();
        p.Who   = PickedVoice();
        p.Title = title;
        p.Body  = body;

        string json;
        string err;
        if (!JsonFileLoader<OZ_NewsPostAsk>.MakeData(p, json, err, false))
        {
            SetHintSticky("NewsHint", "#STR_OZ_ERR_PDA_INTERNAL");
            return;
        }

        SetHint("NewsHint", "#STR_OZ_NEWS_SENDING");
        OZ_Rpc.Request(OZ_PdaConst.PAGE_NEWS, "post", json);
    }

    private void Repaint()
    {
        for (int r = 0; r < m_RowWgts.Count(); r++)
        {
            if (m_RowWgts[r])
                m_RowWgts[r].Unlink();
        }
        m_RowWgts.Clear();

        // The spacer measures itself only on Update(): rows added or removed
        // without it keep the old height until the next relayout.
        if (m_Rows)
            m_Rows.Update();

        int n = 0;
        if (m_List && m_List.Items)
            n = m_List.Items.Count();

        if (n == 0)
        {
            SetHint("NewsHint", "#STR_OZ_NEWS_EMPTY");
            return;
        }

        for (int i = 0; i < n; i++)
        {
            OZ_NewsItem it = m_List.Items[i];

            Widget row = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_news_row.layout", m_Rows);
            if (!row)
                break;

            row.SetName(it.Id);
            row.SetUserID(7);
            m_RowWgts.Insert(row);

            TextWidget t = TextWidget.Cast(row.FindAnyWidget("RowTitle"));
            if (t)
                t.SetText(it.Title);

            TextWidget meta = TextWidget.Cast(row.FindAnyWidget("RowMeta"));
            if (meta)
                meta.SetText(it.Who + "   " + Day(it.At));

            Widget pick = row.FindAnyWidget("RowPick");
            if (pick)
                pick.Show(it.Id == m_OpenId);
        }

        // The spacer measures itself only on Update(): rows added or removed
        // without it keep the old height until the next relayout.
        if (m_Rows)
            m_Rows.Update();
    }

    // "2026-08-28 01:23:45" -> "28.08". Різати за позиціями чесно: формат
    // задає міст і він сталий.
    private string Day(string at)
    {
        if (at.Length() < 10)
            return at;
        return at.Substring(8, 2) + "." + at.Substring(5, 2);
    }
}
