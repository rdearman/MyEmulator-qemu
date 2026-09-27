-- Give the major Part headings book-style treatment while leaving chapters
-- and appendices as ordinary LaTeX chapters.
function Header(el)
  if el.level == 1 then
    local text = pandoc.utils.stringify(el)
    if text:match('^Part ') then
      return pandoc.RawBlock('latex', '\\part{' .. text .. '}')
    end
    -- The source edition uses H1 for many internal labels. Only actual
    -- chapters, appendices, and the front reference anchors should start a
    -- new book chapter; demote the rest to sections.
    if not (text:match('^Chapter ')
      or text:match('^Appendix ')
      or text:match('^REM ')
      or text:match('^ISA, Platform')
      or text:match('^What Is Implemented')
      or text:match('^Developer Journeys')
      or text:match('^Canonical REM')
      or text:match('^How to use this offline')
      or text:match('^Historical development')
      or text == 'Preface'
      or text == 'Appendices') then
      el.level = 2
    end
  end
  return el
end
