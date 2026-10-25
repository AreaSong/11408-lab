# 由 lab.sh 引入（source），不单独运行：生成项目与关卡的配置、复制依赖、同步。
# 用到 lab.sh 里的 root、die、rel、attempts_of。

tpl_dir="$root/cmake/templates"
warmup_rel="lib/00-warmup"
all_subjects=(lib co os net example)

# 关卡目录名去掉编号、连字符换成下划线，作为程序名前缀：02-linked-list → linked_list
level_base() {
    local b
    b="$(basename "$1")"
    b="${b:3}"
    echo "${b//-/_}"
}

all_levels() {
    local s d
    for s in "${all_subjects[@]}"; do
        for d in "$root/$s"/[0-9][0-9]-*; do
            if [ -d "$d" ]; then echo "$d"; fi
        done
    done
}

# 关卡里能被 CMake 收录的项目（相对关卡的路径）：answer 在前，尝试按日期
projects_of() {
    local a
    if [ -f "$1/answer/CMakeLists.txt" ]; then echo answer; fi
    for a in $(attempts_of "$1"); do
        if [ -f "$1/mine/$a/CMakeLists.txt" ]; then echo "mine/$a"; fi
    done
}

write_level_cmake() {
    local dir="$1" list p
    list="$(projects_of "$dir")"
    {
        echo "# 由 ./lab.sh 生成，新建、改名、删除尝试时自动更新：收录本关的答案和每次尝试"
        echo "cmake_minimum_required(VERSION 3.20)"
        echo "project($(level_base "$dir") C)"
        echo
        echo "enable_testing()"
        if [ -n "$list" ]; then
            echo
            while read -r p; do echo "add_subdirectory($p)"; done <<< "$list"
        fi
    } > "$dir/CMakeLists.txt"
}

# 从 LEVELS.md 摘出本关小节，生成题目占位；已有 TASK.md 时不动
ensure_task() {
    local dir="$1" subject num levels_md section=""
    [ ! -f "$dir/TASK.md" ] || return 0
    subject="$(basename "$(dirname "$dir")")"
    num="$(basename "$dir" | cut -c1-2)"
    levels_md="$root/$subject/LEVELS.md"
    if [ -f "$levels_md" ]; then
        section="$(awk -v head="### $num " '
            /^## / && on { exit }
            /^### / { if (on) exit; if (index($0, head) == 1) on = 1 }
            on' "$levels_md")"
    fi
    [ -n "$section" ] || section="（LEVELS.md 里没有找到本关的说明）"
    {
        sed -n '/@SECTION@/q;p' "$tpl_dir/TASK.md.in" | sed "s|@LEVEL@|$(rel "$dir")|"
        printf '%s\n' "$section"
        sed '1,/@SECTION@/d' "$tpl_dir/TASK.md.in"
    } > "$dir/TASK.md"
}

# ---------- 依赖 ----------

deps_lines() {
    [ -f "$1/DEPS" ] || return 0
    sed -e 's/#.*//' -e 's/[[:space:]]*$//' -e 's/^[[:space:]]*//' "$1/DEPS" | grep -v '^$' || true
}

_deps_seen=""
_resolve_into() {
    local line dep
    while read -r line; do
        [ -n "$line" ] || continue
        dep="${line%% *}"
        case " $_deps_seen " in *" $dep "*) continue ;; esac
        [ -d "$root/$dep" ] || die "$(rel "$1")/DEPS 里写的 ${dep} 不存在"
        _deps_seen="$_deps_seen $dep"
        _resolve_into "$root/$dep"
        echo "$line"
    done <<< "$(deps_lines "$1")"
}

# 递归展开 DEPS，输出「关卡 [文件...]」，每个关卡只出现一次，被依赖的在前
resolve_deps() {
    _deps_seen=""
    _resolve_into "$1"
}

# 依赖关卡提供哪份 src/：答案项目用答案；尝试用该关最新标记完成的尝试，没有就用答案
dep_src_dir() {
    local base="$root/$1" a picked=""
    if [ "$2" = mine ]; then
        for a in $(attempts_of "$base"); do
            if [ -f "$base/mine/$a/.done" ]; then picked="$base/mine/$a/src"; fi
        done
    fi
    echo "${picked:-$base/answer/src}"
}

# copy_dep_files <依赖关卡> <answer|mine> <目标 deps 目录> [文件...]；不列文件就复制整个 src/
copy_dep_files() {
    local dep="$1" kind="$2" dest="$3" src f
    shift 3
    src="$(dep_src_dir "$dep" "$kind")"
    if [ ! -d "$src" ]; then
        echo "提示：${dep} 还没有可用的 src/，$(rel "$dest") 暂缺这部分依赖" >&2
        return 0
    fi
    if [ $# -eq 0 ]; then
        set -- $(cd "$src" && ls | grep -E '\.[ch]$' || true)
    fi
    for f in "$@"; do
        if [ ! -f "$src/$f" ]; then
            echo "提示：$(rel "$src") 里没有 ${f}" >&2
            continue
        fi
        if [ -e "$dest/$f" ]; then echo "提示：$(rel "$dest")/${f} 重名，后复制的 ${dep} 版本覆盖了前一个" >&2; fi
        cp "$src/$f" "$dest/$f"
    done
}

# 重建项目的 deps/：prompt.h + test.h（来自 lib/00）+ DEPS 展开后的各关代码
fill_deps() {
    local level="$1" proj="$1/$2" kind=answer line dep files
    [ "$2" = answer ] || kind=mine
    rm -rf "$proj/deps"
    mkdir -p "$proj/deps"
    cp "$tpl_dir/prompt.h" "$proj/deps/"
    if [ "$(rel "$level")" != "$warmup_rel" ]; then
        copy_dep_files "$warmup_rel" "$kind" "$proj/deps" test.h
    fi
    while read -r line; do
        [ -n "$line" ] || continue
        dep="${line%% *}"
        files=""
        if [ "$line" != "$dep" ]; then files="${line#* }"; fi
        # files 故意不加引号，按空格拆成多个文件名
        copy_dep_files "$dep" "$kind" "$proj/deps" $files
    done <<< "$(resolve_deps "$level")"
}

# ---------- 新项目的模板 ----------

header_guard() { basename "$1" | tr 'a-z' 'A-Z' | tr -c 'A-Z0-9\n' '_'; }

template_main() {
    local h
    printf '#include <stdio.h>\n\n#include "prompt.h"\n'
    for h in "$@"; do printf '#include "%s"\n' "$h"; done
    printf '\nint main(void) {\n    return 0;\n}\n'
}

template_test() {
    # lib/00 的 test.h 是本关自己要写的，空骨架里还没有 TEST_SUMMARY
    if [ -f "$1/src/test.h" ]; then
        printf 'int main(void) {\n    return 0;\n}\n'
    else
        printf '#include "test.h"\n\nint main(void) {\n    return TEST_SUMMARY();\n}\n'
    fi
}

# 程序入口文件排序：main.c、test.c、其余按字母
entry_files() {
    (cd "$1" && ls | grep -E '\.c$' || true) | awk '{ k = ($0 == "main.c") ? 0 : ($0 == "test.c") ? 1 : 2; print k "\t" $0 }' | sort | cut -f2
}

# write_project_cmake <项目目录> <项目名> <tests 相对路径>
write_project_cmake() {
    local dir="$1" name="$2" level srcs f target
    level="$(cd "$dir/$3" && pwd)"
    srcs="$(cd "$dir" && ls src 2>/dev/null | grep -E '\.c$' | sed 's|^|src/|' | tr '\n' ' ' || true)"
    if [ -n "$(deps_lines "$level")" ]; then srcs="$srcs\${DEPS_SRC}"; fi
    srcs="${srcs% }"
    {
        echo "cmake_minimum_required(VERSION 3.20)"
        echo "project($name C)"
        echo
        echo "set(CMAKE_C_STANDARD 11)"
        echo "add_compile_options(-Wall -Wextra)"
        echo "include_directories(src deps)"
        if [ -n "$(deps_lines "$level")" ]; then
            echo "file(GLOB DEPS_SRC CONFIGURE_DEPENDS deps/*.c)"
        fi
        echo
        for f in $(entry_files "$dir"); do
            case "$f" in
                main.c) target="$name" ;;
                test.c) target="${name}_test" ;;
                *) target="${name}_$(basename "$f" .c)" ;;
            esac
            echo "add_executable($target $f${srcs:+ $srcs})"
        done
        echo
        echo "include($3/tests/check.cmake OPTIONAL)"
    } > "$dir/CMakeLists.txt"
}

# create_project <关卡目录> <项目目录> <项目名>：照答案的文件结构生成能直接编译的空骨架
create_project() {
    local level="$1" dir="$2" name="$3" answer="$1/answer" f headers=""
    mkdir -p "$dir/src"
    if [ -d "$answer/src" ]; then
        for f in $(cd "$answer/src" && ls | grep -E '\.h$' || true); do
            printf '#ifndef %s\n#define %s\n\n#endif\n' "$(header_guard "$f")" "$(header_guard "$f")" > "$dir/src/$f"
            headers="$headers $f"
        done
        for f in $(cd "$answer/src" && ls | grep -E '\.c$' || true); do
            if [ -f "$dir/src/${f%.c}.h" ]; then
                printf '#include "%s.h"\n' "${f%.c}" > "$dir/src/$f"
            else
                : > "$dir/src/$f"
            fi
        done
    fi
    template_main $headers > "$dir/main.c"
    template_test "$dir" > "$dir/test.c"
    if [ -d "$answer" ]; then
        for f in $(entry_files "$answer"); do
            case "$f" in
                main.c|test.c) ;;
                *) printf 'int main(void) {\n    return 0;\n}\n' > "$dir/$f" ;;
            esac
        done
    fi
    write_project_cmake "$dir" "$name" "../.."
}

# ---------- 同步 ----------

# 关卡的公共部分 + 每个项目的 deps/ + 关卡 CMakeLists；不碰项目自己的 CMakeLists 和代码
sync_level() {
    local dir="$1" p
    mkdir -p "$dir/tests"
    cp "$tpl_dir/check.cmake" "$dir/tests/check.cmake"
    ensure_task "$dir"
    for p in $(projects_of "$dir"); do fill_deps "$dir" "$p"; done
    write_level_cmake "$dir"
}

# 某关的完成标记变了：重建依赖它（含间接依赖）的关卡里各次尝试的 deps/
sync_dependents() {
    local changed d p
    changed="$(rel "$1")"
    for d in $(all_levels); do
        [ "$d" != "$1" ] || continue
        # 不用 grep -q：它提前退出会让上游收到 SIGPIPE，pipefail 下整条判断变成失败
        if [ "$changed" = "$warmup_rel" ] || resolve_deps "$d" | grep -E "^${changed}( |\$)" >/dev/null; then
            for p in $(projects_of "$d" | grep '^mine/' || true); do
                fill_deps "$d" "$p"
                echo "已更新依赖：$(rel "$d/$p")/deps/"
            done
        fi
    done
}
