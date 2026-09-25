#include "helper.h"

LPCTSTR nsCommon::qstringToLpcstr(const QString &qstr)
{
    std::wstring wstr = qstr.toStdWString();

    const wchar_t* lpwstr = reinterpret_cast<const wchar_t*>(qstr.utf16());

    return lpwstr;
}
