// Сторінка «Новини»: список постів зліва, тіло обраного справа.
//
// Читалка й нічого більше: писати сюди можна лише з Discord, і сторінка
// чесно не має жодної кнопки вводу.

class OZ_PdaPageNews : OZ_PdaPage
{
    private Widget m_Rows;
    private ref array<Widget> m_RowWgts;
    private ref OZ_NewsList m_List;
    private string m_OpenId = "";

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_news.layout";
    }

    override void OnBuilt()
    {
        m_Rows    = Wgt("NewsRows");
        m_RowWgts = new array<Widget>();
    }

    override void OnSelected()
    {
        ClearHintHold();
        OZ_Rpc.Request(OZ_PdaConst.PAGE_NEWS, "list", "{}");
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

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
            SetHintSticky("NewsHint", "#" + error);
            return;
        }

        string err;

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

            SetHint("NewsHint", "");
            return;
        }
    }

    private void Repaint()
    {
        for (int r = 0; r < m_RowWgts.Count(); r++)
        {
            if (m_RowWgts[r])
                m_RowWgts[r].Unlink();
        }
        m_RowWgts.Clear();

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
