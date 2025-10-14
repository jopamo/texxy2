/*
 * texxy/highlighter/highlighter-patterns.cpp
 */

#include "highlighter.h"

namespace Texxy {

QStringList Highlighter::keywords(const QString& lang) {
    QStringList keywordPatterns;
    if (lang == "c" || lang == "cpp") {
        keywordPatterns << "\\b(and|asm|auto)(?!(\\.|-|@|#|\\$))\\b"
                        << "\\b(const|case|catch|cdecl|continue)(?!(\\.|-|@|#|\\$))\\b"
                        << "\\b(break|default|do)(?!(\\.|-|@|#|\\$))\\b"
                        << "\\b(enum|explicit|else|extern)(?!(\\.|-|@|#|\\$))\\b"
                        << "\\b(for|goto|if|not|NULL|or|pasca|register|return)(?!(\\.|-|@|#|\\$))\\b"
                        << "\\b(signals|sizeof|static|struct|switch)(?!(\\.|-|@|#|\\$))\\b"
                        << "\\b(typedef|typename|union|volatile|while)(?!(\\.|-|@|#|\\$))\\b";

        if (lang == "c")
            keywordPatterns << "\\b(FALSE|TRUE)(?!(\\.|-|@|#|\\$))\\b";
        else
            keywordPatterns
                << "\\b(class|constexpr|const_cast|delete|dynamic_cast)(?!(\\.|-|@|#|\\$))\\b"
                << "\\b(false|foreach|friend|inline|namespace|new|operator)(?!(\\.|-|@|#|\\$))\\b"
                << "\\b(nullptr|override|private|protected|public|qobject_cast|reinterpret_cast|slots|static_"
                   "cast)(?!(\\.|-|@|#|\\$))\\b"
                << "\\b(template|true|this|throw|try|typeid|using|virtual)(?!(\\.|-|@|#|\\$))\\b"
                << "\\bthis(?=->)\\b";  // "this" can be followed by "->"
    }
    else if (lang == "sh" || lang == "makefile" ||
             lang == "cmake")  // the characters "(", ";" and "&" will be reformatted after this
    {
        keywordPatterns
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "alias|bg|bind|break|builtin)(?!(\\.|-|@|#|\\$))\\b"
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "caller|case|command|compgen|complete|continue)(?!(\\.|-|@|#|\\$))\\b"
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "declare|default|dirs|disown|do|done)(?!(\\.|-|@|#|\\$))\\b"
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "echo|elif|elseif|else|enable|esac|eval|exec|exit|export)(?!(\\.|-|@|#|\\$))\\b"
            << "\\b(?<!(@|\\^|\\$|\\.|%|\\*|\\+|-|/))false(?!(\\.|-|@|#|\\$))\\b"
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "fg|fi|for|foreach|function|getopts|hash|help|history|if|in)(?!(\\.|-|@|#|\\$))\\b"
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "jobs|let|local|login|logout|popd|printf|pushd|read|readonly|return)(?!(\\.|-|@|#|\\$))\\b"
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "select|seq|set|shift|shopt|source|suspend|test|then|times|trap|type|typeset)(?!(\\.|-|@|#|\\$))\\b"
            << "\\b(?<!(@|\\^|\\$|\\.|%|\\*|\\+|-|/))true(?!(\\.|-|@|#|\\$))\\b"
            << "((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)"
               "*)("
               "umask|unalias|unset|until|wait|while)(?!(\\.|-|@|#|\\$))\\b";
        if (lang == "cmake")  // (?i) is supported by QRegularExpression for ignoring case-sensitivity
            keywordPatterns
                << "(?i)(^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)(endif|endmacro|endwhile|file|include|option|"
                   "project|"
                   "add_compile_options|add_custom_command|add_custom_target|add_definitions|add_dependencies|add_"
                   "executable|"
                   "add_library|add_subdirectory|add_feature_info|add_test|aux_source_directory|build_command|cmake_"
                   "dependent_option|cmake_host_system_information|cmake_minimum_required|cmake_parse_arguments|cmake_"
                   "policy|"
                   "configure_file|configure_package_config_file|create_test_sourcelist|define_property|ecm_configure_"
                   "package_config_file|ecm_setup_version|enable_language|enable_testing|endforeach|endfunction|exec_"
                   "program|"
                   "execute_process|feature_summary|find_file|find_library|find_package|find_package_handle_standard_"
                   "args|"
                   "find_path|find_program|fltk_wrap_ui|function|get_cmake_property|get_directory_property|get_"
                   "filename_"
                   "component|get_property|get_source_file_property|get_target_property|get_test_property|include_"
                   "directories|include_external_msproject|include_regular_expression|link_directories|list|load_cache|"
                   "load_"
                   "command|macro|mark_as_advanced|math|message|pkg_check_modules|pkg_search_module|print_enabled_"
                   "features|"
                   "print_disabled_features|qt_wrap_cpp|qt_wrap_ui|remove_definitions|separate_arguments|set_directory_"
                   "properties|set_feature_info|set_package_properties|set_property|set_source_files_properties|set_"
                   "target_"
                   "properties|set_tests_properties|site_name|source_group|string|target_compile_definitions|target_"
                   "compile_"
                   "options|target_include_directories|target_link_libraries|try_compile|try_run|variable_watch|write_"
                   "basic_"
                   "package_version_file)\\s*(?=(\\(|$))";
    }
    else if (lang == "qmake") {
        keywordPatterns
            << "\\b(CONFIG|DEFINES|DEF_FILE|DEPENDPATH|DEPLOYMENT_PLUGIN|DESTDIR|DISTFILES|DLLDESTDIR|FORMS|GUID|"
               "HEADERS|"
               "ICON|IDLSOURCES|INCLUDEPATH|INSTALLS|LEXIMPLS|LEXOBJECTS|LEXSOURCES|LIBS|LITERAL_HASH|MAKEFILE|"
               "MAKEFILE_"
               "GENERATOR|MOC_DIR|OBJECTS|OBJECTS_DIR|PKGCONFIG|POST_TARGETDEPS|PRE_TARGETDEPS|PRECOMPILED_HEADER|PWD|"
               "OTHER_FILES|OUT_PWD|QMAKE|QMAKESPEC|QMAKE_AR_CMD|QMAKE_BUNDLE_DATA|QMAKE_BUNDLE_EXTENSION|QMAKE_BUNDLE_"
               "EXTENSION|QMAKE_CFLAGS|QMAKE_CFLAGS|QMAKE_CFLAGS_RELEASE|QMAKE_CFLAGS_SHLIB|QMAKE_CFLAGS_THREAD|QMAKE_"
               "CFLAGS_WARN_OFF|QMAKE_CFLAGS_WARN_ON|QMAKE_CLEAN|QMAKE_CXX|QMAKE_CXXFLAGS|QMAKE_CXXFLAGS_DEBUG|QMAKE_"
               "CXXFLAGS_RELEASE|QMAKE_CXXFLAGS_SHLIB|QMAKE_CXXFLAGS_THREAD|QMAKE_CXXFLAGS_WARN_OFF|QMAKE_CXXFLAGS_"
               "WARN_ON|"
               "QMAKE_DISTCLEAN|QMAKE_EXTENSION_SHLIB|QMAKE_EXTENSION_STATICLIB|QMAKE_EXT_MOC|QMAKE_EXT_UI|QMAKE_EXT_"
               "PRL|"
               "QMAKE_EXT_LEX|QMAKE_EXT_YACC|QMAKE_EXT_OBJ|QMAKE_EXT_CPP|QMAKE_EXT_H|QMAKE_EXTRA_COMPILERS|QMAKE_EXTRA_"
               "TARGETS|QMAKE_FAILED_REQUIREMENTS|QMAKE_FRAMEWORK_BUNDLE_NAME|QMAKE_FRAMEWORK_VERSION|QMAKE_HOST|QMAKE_"
               "INCDIR|QMAKE_INCDIR_EGL|QMAKE_INCDIR_OPENGL|QMAKE_INCDIR_OPENGL_ES2|QMAKE_INCDIR_OPENVG|QMAKE_INCDIR_"
               "X11|"
               "QMAKE_INFO_PLIST|QMAKE_LFLAGS|QMAKE_LFLAGS_CONSOLE|QMAKE_LFLAGS_DEBUG|QMAKE_LFLAGS_PLUGIN|QMAKE_LFLAGS_"
               "RPATH|QMAKE_LFLAGS_REL_RPATH|QMAKE_REL_RPATH_BASE|QMAKE_LFLAGS_RPATHLINK|QMAKE_LFLAGS_RELEASE|QMAKE_"
               "LFLAGS_"
               "APP|QMAKE_LFLAGS_SHLIB|QMAKE_LFLAGS_SONAME|QMAKE_LFLAGS_THREAD|QMAKE_LFLAGS_WINDOWS|QMAKE_LIBDIR|QMAKE_"
               "LIBDIR_FLAGS|QMAKE_LIBDIR_EGL|QMAKE_LIBDIR_OPENGL|QMAKE_LIBDIR_OPENVG|QMAKE_LIBDIR_X11|QMAKE_LIBS|"
               "QMAKE_"
               "LIBS_EGL|QMAKE_LIBS_OPENGL|QMAKE_LIBS_OPENGL_ES1|QMAKE_LIBS_OPENGL_ES2|QMAKE_LIBS_OPENVG|QMAKE_LIBS_"
               "THREAD|"
               "QMAKE_LIBS_X11|QMAKE_LIB_FLAG|QMAKE_LINK_SHLIB_CMD|QMAKE_LN_SHLIB|QMAKE_OBJECTIVE_CFLAGS|QMAKE_POST_"
               "LINK|"
               "QMAKE_PRE_LINK|QMAKE_PROJECT_NAME|QMAKE_MAC_SDK|QMAKE_MACOSX_DEPLOYMENT_TARGET|QMAKE_MAKEFILE|QMAKE_"
               "QMAKE|"
               "QMAKE_RESOURCE_FLAGS|QMAKE_RPATHDIR|QMAKE_RPATHLINKDIR|QMAKE_RUN_CC|QMAKE_RUN_CC_IMP|QMAKE_RUN_CXX|"
               "QMAKE_"
               "RUN_CXX_IMP|QMAKE_SONAME_PREFIX|QMAKE_TARGET|QMAKE_TARGET_COMPANY|QMAKE_TARGET_DESCRIPTION|QMAKE_"
               "TARGET_"
               "COPYRIGHT|QMAKE_TARGET_PRODUCT|QT|QTPLUGIN|QT_VERSION|QT_MAJOR_VERSION|QT_MINOR_VERSION|QT_PATCH_"
               "VERSION|"
               "RC_CODEPAGE|RC_DEFINES|RCC_DIR|RC_FILE|RC_ICONS|RC_INCLUDEPATH|RC_LANG|REQUIRES|RES_FILE|RESOURCES|"
               "SIGNATURE_FILE|SOURCES|SUBDIRS|TARGET|TARGET_EXT|TARGET_x|TARGET_x.y.z|TEMPLATE|TRANSLATIONS|UI_DIR|"
               "VERSION|VERSION_PE_HEADER|VER_MAJ|VER_MIN|VER_PAT|VPATH|WINRT_MANIFEST|YACCSOURCES|_PRO_FILE_|_PRO_"
               "FILE_"
               "PWD_)(?!(@|#|\\$))\\b";
    }
    else if (lang == "troff") {
        keywordPatterns
            << "^\\.(AT|B|BI|BR|BX|CW|DT|EQ|EN|I|IB|IR|IP|LG|LP|NL|P|PE|PD|PP|PS|R|RI|RB|RS|RE|SH|SM|SB|SS|TH|TS|TE|HP|"
               "TP|"
               "UC|UL|ab|ad|af|am|as|bd|bp|br|brp|c2|cc|ce|cend|cf|ch|cs|cstart|cu|da|de|di|ds|dt|ec|el|em|end|eo|ev|"
               "ex|fc|"
               "fi|fl|fp|ft|hc|hw|hy|ie|if|ig|in|it|lc|lg|ll|ls|lt|mc|mk|na|ne|nf|nh|nm|nn|nr|ns|nx|os|pc|pi|pl|pm|pm|"
               "pn|"
               "po|ps|rd|rj|rm|rn|rr|rs|rt|so|sp|ss|sv|sy|ta|tc|ti|tl|tm|tr|uf|ul|vs|wh)(?!(\\.|-|@|#|\\$))\\b";
    }
    else if (lang == "LaTeX") {
        keywordPatterns << "(?<!\\\\)\\\\(documentclass|begin|end|usepackage|maketitle|include|includeonly|input|label|"
                           "caption|ref|pageref)(?!(@|#))(_|\\b)"
                        << "(?<!\\\\)\\\\(section|subsection|subsubsection|title|author|part|chapter|paragraph|"
                           "subparagraph)\\*?\\s*\\{[^{}]*\\}";
    }
    else if (lang == "perl") {
        keywordPatterns
            << "\\b(?<!(@|#|%|\\$))(abs|accept|alarm|and|atan2|BEGIN|bind|binmode|bless|break|bytes)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(caller|chdir|chmod|chown|chroot|chomp|chop|chr|close|closedir|cmp|connect|constant|"
               "continue|cos|crypt)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(dbmclose|dbmopen|default|defined|delete|diagnostics|die|do|dump)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(each|else|elsif|endgrent|END|endhostent|endnetent|endprotoent|endpwent|endservent|"
               "english|eof|eq|eval|exec|exists|exit|exp)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(fcntl|fileno|filetest|flock|for|foreach|fork|format|formline)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(ge|getc|getgrent|getgrgid|getgrnam|gethostbyaddr|gethostbyname|gethostent|getlogin|"
               "getnetbyaddr|getnetbyname|getnetent|getpeername|getpgrp|getppid|getpriority|getprotobyname|"
               "getprotobynumber|getprotoent|getpwent|getpwnam|getpwuid|getservbyname|getservbyport|getservent|"
               "getsockname|"
               "getsockopt|given|glob|gmtime|goto|grep|gt|hex)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(if|import|index|int|integer|ioctl|join|keys|kill)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(last|lc|lcfirst|le|length|less|link|listen|local|locale|localtime|lock|log|lt|"
               "lstat|"
               "map|mkdir|msgctl|msgget|msgrcv|msgsnd|my|ne|next|no|not)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(oct|open|opendir|or|ord|our|pack|package|pipe|pop|pos|print|printf|prototype|push|"
               "quotemeta)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(rand|read|readdir|readline|readlink|readpipe|recv|redo|ref|rename|require|return|"
               "reverse|rewinddir|rindex|rmdir)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(say|scalar|seek|seekdir|select|semctl|semget|semop|send|setgrent|sethostent|"
               "setnetent|"
               "setpgrp|setpriority|setprotoent|setpwent|setservent|setsockopt|shift|shmctl|shmget|shmread|shmwrite|"
               "shutdown|sigtrap|sin|sleep|socket|socketpair|sort|splice|split|sprintf|sqrt|srand|stat|state|strict|"
               "study|"
               "sub|subs|substr|symlink|syscall|sysopen|sysread|sysseek|system|syswrite|switch)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(tell|telldir|tie|tied|time|times|truncate)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(uc|ucfirst|umask|undef|unless|unlink|unpack|unshift|untie|until|use|utf8|utime)(?!("
               "@|#|"
               "\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(vars|values|vec)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))(wait|waitpid|warn|warnings|wantarray|when|while|write|xor)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|%|\\$))__(FILE|LINE|PACKAGE)__";
    }
    else if (lang == "ruby") {
        keywordPatterns
            << "\\b(__FILE__|__LINE__)(?!(@|#|\\$))\\b"
            << "\\b(alias|and|begin|BEGIN|break)(?!(@|#|\\$))\\b"
            << "\\b(case|class|defined|do|def)(?!(@|#|\\$))\\b"
            << "\\b(else|elsif|end|END|ensure|for|false)(?!(@|#|\\$))\\b"
            << "\\b(if|in|module)(?!(@|#|\\$))\\b"
            << "\\b(next|nil|not|private|private_class_method|protected|public|public_class_method|or)(?!(@|#|\\$))\\b"
            << "\\b(redo|rescue|retry|return)(?!(@|#|\\$))\\b"
            << "\\b(super|self|then|true)(?!(@|#|\\$))\\b"
            << "\\b(undef|unless|until|when|while|yield)(?!(@|#|\\$))\\b";
    }
    else if (lang == "lua")
        keywordPatterns << "\\b(and|break|do)(?!(\\.|@|#|\\$))\\b"
                        << "\\b(else|elseif|end)(?!(\\.|@|#|\\$))\\b"
                        << "\\b(false|for|function|goto)(?!(\\.|@|#|\\$))\\b"
                        << "\\b(if|in|local|nil|not|or|repeat|return)(?!(\\.|@|#|\\$))\\b"
                        << "\\b(then|true|until|while)(?!(\\.|@|#|\\$))\\b";
    else if (lang == "python")
        keywordPatterns
            << "\\b(__debug__|__file__|__name__|and|as|assert|async|await|break|class|continue)(?!(@|\\$))\\b"
            << "\\b(def|del|elif|Ellipsis|else|except|False|finally|for|from|global)(?!(@|\\$))\\b"
            << "\\b(if|is|import|in|lambda|None|nonlocal|not|NotImplemented|or|pass|raise|return|True|try|while|with|"
               "yield)"
               "(?!(@|\\$))\\b"
            << "\\b(ArithmeticError|AssertionError|AttributeError|BaseException|BlockingIOError|BrokenPipeError|"
               "BufferError|BytesWarning|ChildProcessError|ConnectionAbortedError|ConnectionError|"
               "ConnectionRefusedError|"
               "ConnectionResetError|DeprecationWarning|EnvironmentError|EOFError|Exception|FileExistsError|"
               "FileNotFoundError|FloatingPointError|FutureWarning|GeneratorExit|ImportError|ImportWarning|"
               "IndentationError|IndexError|InterruptedError|IOError|IsADirectoryError|KeyboardInterrupt|KeyError|"
               "LookupError|MemoryError|NameError|NotADirectoryError|NotImplementedError|OSError|OverflowError|"
               "PendingDeprecationWarning|PermissionError|ProcessLookupError|ReferenceError|ResourceWarning|"
               "RuntimeError|"
               "RuntimeWarning|StandardError|StopIteration|SyntaxError|SyntaxWarning|SystemError|SystemExit|TabError|"
               "TimeoutError|TypeError|UnboundLocalError|UnicodeDecodeError|UnicodeEncodeError|UnicodeError|"
               "UnicodeTranslateError|UnicodeWarning|UserWarning|ValueError|Warning|WindowsError|ZeroDivisionError)(?!("
               "@|"
               "\\$))\\b"
            << "\\b(exec|print)(?!(@|\\$|\\s*\\())\\b";
    else if (lang == "javascript" || lang == "qml") {
        keywordPatterns << "\\b(?<!(@|#|\\$))(abstract|arguments|await|async|break)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(case|catch|class|const|continue)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(debugger|default|delete|do)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(else|enum|eval|extends)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(false|final|finally|for|function|goto)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(if|implements|in|Infinity|instanceof|interface|let)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(Math|NaN|native|new|null|of)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(package|private|protected|prototype|public|return)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(static|super|switch|synchronized)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(throw|throws|this|transient|true|try|typeof)(?!(@|#|\\$))\\b"
                        << "\\b(?<!(@|#|\\$))(undefined|void|volatile|while|with|yield)(?!(@|#|\\$))\\b";
        if (lang == "javascript")
            keywordPatterns << "\\b(?<!(@|#|\\$))(var)(?!(@|#|\\$))\\b";
        else if (lang == "qml")
            keywordPatterns << "\\b(?<!(@|#|\\$))(alias|id|property|readonly|signal)(?!(@|#|\\$))\\b";
    }
    else if (lang == "php")
        keywordPatterns
            << "\\b(?<!(#|\\$))(__FILE__|__LINE__|__FUNCTION__|__CLASS__|__COMPILER_HALT_OFFSET__|__METHOD__|__"
               "DIR__|__NAMESPACE__|__TRAIT__)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(and|abstract|array|as|bool|break)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(callable|case|catch|cfunction|class|clone|const|continue)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(declare|default|die|do)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(each|echo|else|elseif|empty|enddeclare|endfor|endforeach|endif|endswitch|"
               "endwhile|eval|exception|exit|extends)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(false|final|finally|float|for|foreach|function)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(global|goto|if|implements|include|include_once|insteadof|int|interface|"
               "instanceof|isset|iterable)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(list|mixed|namespace|new|null|numeric|object|old_function|or)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(php_user_filter|print|private|protected|public|require|require_once|resource|"
               "return)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(static|string|switch|throw|trait|true|try)(?!(#|\\$))\\b"
            << "\\b(?<!(#|\\$))(unset|use|var|void|while|xor|yield)(?!(#|\\$))\\b";
    else if (lang == "scss")  // taken from http://sass-lang.com/documentation/Sass/Script/Functions.html
        keywordPatterns
            << "\\b(none|null)(?!(\\.|-|@|#|\\$))\\b"
            << "\\b(abs|adjust-color|adjust-hue|alpha|append|blue|call|ceil|change-color|comparable|complement|content-"
               "exists|darken|desaturate|feature-exists|floor|function-exists|get-function|global-variable-exists|"
               "grayscale|green|hsl|hsla|hue|ie-hex-str|if|index|inspect|invert|is-bracketed|is-superselector|join|"
               "keywords|length|lighten|lightness|list-separator|map-get|map-has-key|map-keys|map-merge|map-remove|map-"
               "values|max|min|mixin-exists|mix|nth|opacify|percentage|quote|random|red|rgb|rgba|round|scale-color|"
               "saturate|saturation|selector-nest|selector-append|selector-extend|selector-parse|selector-replace|"
               "selector-"
               "unify|set-nth|simple-selectors|str-index|str-insert|str-length|str-slice|to-lower-case|to-upper-case|"
               "transparentize|type-of|unit|unitless|unquote|variable-exists|zip)(?=\\()"
            << "\\bunique-id\\(\\s*\\)";
    else if (lang == "dart")
        keywordPatterns
            << "\\b(?<!(@|#|\\$))(abstract|as|assert|async|await|break|case|catch|class|const|continue|"
               "covariant|default|deferred|do|dynamic)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|\\$))(else|enum|export|extends|extension|external|factory|false|final|finally|for|"
               "Function|get|hide|if|implements|import|in|interface|is)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|\\$))(late|library|mixin|new|null|on|operator|part|rethrow|return|set|show|static|"
               "super|switch|sync)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|\\$))(this|throw|true|try|typedef|var|void|while|with|yield)(?!(@|#|\\$))\\b"
            << "(?<!(@|#|\\$|\\w))(@pragma|@override|@deprecated)(?!(@|#|\\$))\\b";
    else if (lang == "pascal")  // case-insensitive
        keywordPatterns
            << "(?i)\\b(?<!(@|#|\\$))(absolute|abstract|alias|and|array|as|asm|assembler|at|attribute|automated|begin|"
               "bindable|bitpacked|break|case|cdecl|class|const|constructor|continue|cppdecl|cvar|default|deprecated|"
               "destructor|dispinterface|dispose|div|do|downto|dynamic|else|end|enumerator|except|exit|experimental|"
               "export|"
               "exports|external|false|far|far16|file|finalization|finally|for|forward|function|generic|goto|helper|if|"
               "implementation|implements|in|index|inherited|initialization|inline|interface|interrupt|is|label|"
               "library|"
               "iocheck|local|message|mod|module|name|near|new|nil|nodefault|noreturn|nostackframe|not|object|"
               "oldfpccall|"
               "on|only|of|otherwise|out|operator|or|overload|override|pack|packed|page|pascal|platform|private|"
               "procedure|"
               "program|property|protected|public|published|qualified|raise|read|readln|record|register|reintroduce|"
               "repeat|"
               "resourcestring|reset|restricted|result|rewrite|safecall|saveregisters|self|set|shl|shr|softfloat|"
               "specialize|static|stdcall|stored|strict|then|to|threadvar|true|try|type|unaligned|unimplemented|unit|"
               "unpack|until|uses|var|varargs|virtual|while|winapi|with|write|writeln|xor)(?!(@|#|\\$))\\b";
    else if (lang == "java")
        keywordPatterns
            << "\\b(abstract|assert|break|case|catch|class|const|while|continue|default|do|else|enum|extends|"
               "final|finally|for|goto|if|implements|import|instanceof|interface|module|native|new|package|"
               "private|protected|public|return|static|strictfp|super|switch|synchronized|this|throw|throws|"
               "transient|try|var|volatile|while)(?!(@|#|\\$))\\b"
            << "\\b(true|false|null)(?!(\\.|-|@|#|\\$))\\b";
    else if (lang == "go")
        keywordPatterns
            << "\\b(break|case|chan|const|continue|default|defer|else|fallthrough|false|for|func|go|goto|if|import|"
               "interface|iota|map|nil|package|range|return|select|struct|switch|true|type|var)(?!(\\.|-|@|#|\\$))\\b";
    else if (lang == "rust")
        keywordPatterns
            << "\\b(?<!(\\\"|@|#|\\$))(abstract|alignof|as|async|await|become|box|break|const|continue|crate|"
               "default|do|dyn|else|enum|extern|final|fn|for|if|impl|in|let|loop|match|macro|mod|move|mut|"
               "offsetof|override|priv|proc|pub|pure|ref|return|sizeof|static|struct|super|trait|try|type|"
               "typeof|union|unsafe|unsized|use|virtual|where|while|yield)(?!(\\\"|'|@|#|\\$))\\b";
    else if (lang == "tcl")  // backslash should also be taken into account (in a complex way)
        keywordPatterns
            << "(?<!\\\\)(\\\\{2})*(?<!((#|\\$|@|\"|\'|`)(?!\\\\)))(\\\\(#|\\$|@|\"|\'|`)){0,1}\\K\\b("
               "after|append|AppleScript|apply|argc|argv|array|auto_execk|auto_execok|auto_import|auto_load|auto_load_"
               "index|auto_mkindex|auto_mkindex_old|auto_path|auto_qualify|auto_reset|binary|bgerror|break|case|catch|"
               "cd|"
               "clock|close|concat|continue|dde|dict|else|elseif|encoding|env|eof|error|errorCode|errorInfo|eval|exec|"
               "exit|"
               "expr|fblocked|fconfigure|fcopy|file|fileevent|flush|for|foreach|format|gets|glob|global|history|if|"
               "info|"
               "interp|join|lappend|lindex|linsert|list|llength|lmap|load|lrange|lremove|lrepeat|lreplace|lreverse|"
               "lsearch|"
               "lset|lsort|my|namespace|next|nextto|open|package|parray|pid|pkg_mkIndex|prefix|proc|puts|pwd|read|"
               "regexp|"
               "regsub|rename|resource|return|scan|seek|set|socket|source|split|string|subst|switch|tcl_library|tcl_"
               "patchLevel|tcl_pkgPath|tcl_platform|tcl_precision|tcl_rcFileName|tcl_rcRsrcName|tcl_traceCompile|tcl_"
               "traceExec|tcl_version|tclLog|tell|timerate|throw|time|trace|trap|try|unknown|unload|unset|update|"
               "uplevel|"
               "upvar|variable|vwait|while|yield|yieldto"
               "|"
               "bell|bind|bindtags|bitmap|busy|button|canvas|checkbutton|clipboard|destroy|entry|event|focus|font|"
               "fontchooser|frame|grab|grid|image|label|listbox|loadTk|lower|menu|menubutton|message|option|options|"
               "OptProc|pack|photo|place|radiobutton|raise|scale|scrollbar|selection|send|text|tcl_endOfWord|tcl_"
               "findLibrary|tcl_startOfNextWord|tcl_startOfPreviousWord|tcl_wordBreakAfter|tcl_wordBreakBefore|tk|"
               "tkTabToWindow|tk_bisque|tk_chooseColor|tk_chooseDirectory|tk_dialog|tk_focusFollowMouse|tk_focusNext|"
               "tk_"
               "focusPrev|tk_getOpenFile|tk_getSaveFile|tk_library|tk_menuSetFocus|tk_messageBox|tk_optionMenu|tk_"
               "patchLevel|tk_popup|tk_setPalette|tk_strictMotif|tk_textCopy|tk_textCut|tk_textPaste|tk_version|"
               "tkerror|"
               "tkvars|tkwait|toplevel|winfo|wm"
               "|"
               "beep|button|chan|checkbutton|combobox|console|entry|frame|incr|label|labelframe|lassign|menubutton|"
               "notebook|panedwindow|progressbar|radiobutton|registry|scale|scrollbar|separator|sizegrip|spinbox|style|"
               "traverseTo|treeview"
               ")(?!(@|#|\\$|\"|\'|`))\\b";
    else if (lang == "toml")
        keywordPatterns << "\\b(false|inf|nan|true)(?!(\\.|-|@|#|\\$))\\b";

    return keywordPatterns;
}
/*************************/
QStringList Highlighter::types() {
    QStringList typePatterns;
    if (progLan == "c" || progLan == "cpp") {
        typePatterns << "\\b(bool|char|clock_t|double|float|FILE)(?!(\\.|-|@|#|\\$))\\b"
                     << "\\b(gchar|gint|guint(8|16|32|64)?|gboolean)(?!(\\.|-|@|#|\\$))\\b"
                     << "\\b(int(8_t|16_t|32_t|64_t)?|ptrdiff_t|long|short|size_t|ssize_t|time_t)(?!(\\.|-|@|#|\\$))\\b"
                     << "\\b(unsigned|uint(8|16|32|64|8_t|16_t|32_t|64_t)?)(?!(\\.|-|@|#|\\$))\\b"
                     << "\\b(uid_t|gid_t|mode_t)(?!(\\.|-|@|#|\\$))\\b"
                     << "\\b(void|wchar_t|wint_t)(?!(\\.|-|@|#|\\$))\\b";
        if (progLan == "cpp")
            typePatterns << "\\b(qreal|qint8|quint8|qint16|quint16|qint32|quint32|qint64|quint64|qlonglong|qulonglong|"
                            "qptrdiff|quintptr)(?!(\\.|-|@|#|\\$))\\b"
                         << "\\b(uchar|ulong|ushort)(?!(\\.|-|@|#|\\$))\\b"
                         << "\\b(std::[a-z_]+)(?=\\s*\\S+)(?!(\\s*\\(|\\.|-|@|#|\\$))\\b";
    }
    else if (progLan == "qml") {
        typePatterns
            << "\\b(?<!(@|#|\\$))(bool|double|enumeration|int|list|real|string|url|var|variant)(?!(@|#|\\$))\\b"
            << "\\b(?<!(@|#|\\$))(color|date|font|matrix4x4|point|quaternion|rect|size|vector2d|vector3d|vector4d)"
               "(?!(@|#|\\$))\\b";
    }
    else if (progLan == "dart") {
        typePatterns << "\\b(?<!(@|#|\\$))(bool|double|int|num)(?!(@|#|\\$))\\b";
    }
    else if (progLan == "pascal") {
        typePatterns
            << "(?i)\\b(?<!(@|#|\\$))(ansichar|ansistring|byte|cardinal|char|comp|currency|double|dword|extended|int64|"
               "integer|pointer|qword|qwordbool|real|real48|boolean|bytebool|enumerated|longbool|longint|longword|"
               "shortint|"
               "shortstring|single|smallint|string|text|variant|widechar|widestring|word|wordbool)(?!(@|#|\\$))\\b";
    }
    else if (progLan == "java") {
        typePatterns << "\\b(boolean|byte|char|double|float|int|long|short|void)(?!(\\.|-|@|#|\\$))\\b";
    }
    else if (progLan == "go") {
        typePatterns << "\\b(bool|byte|complex64|complex128|error|float32|float64|int8|int16|int32|int64|uint8|uint16|"
                        "uint32|uint64|int|uint|rune|string|uintptr)(?!(\\.|-|@|#|\\$))\\b";
    }
    else if (progLan == "rust") {
        typePatterns << "\\b(?<!(\\\"|@|#|\\$))(bool|isize|usize|i8|i16|i32|i64|i128|u8|u16|u32|u64|u128|f32|f64|char|"
                        "str|Option|"
                        "Result|Self|Box|Vec|String|Path|PathBuf|c_float|c_double|c_void|FILE|fpos_t|DIR|dirent|c_char|"
                        "c_schar|c_"
                        "uchar|c_short|c_ushort|c_int|c_uint|c_long|c_ulong|size_t|ptrdiff_t|clock_t|time_t|c_longlong|"
                        "c_ulonglong|"
                        "intptr_t|uintptr_t|off_t|dev_t|ino_t|pid_t|mode_t|ssize_t)(?!(\\\"|'|@|#|\\$))\\b";
    }

    return typePatterns;
}

}  // namespace Texxy
