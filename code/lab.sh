#!/bin/bash
# 关卡尝试管理：每次尝试是一个能单独用 CLion 打开的项目（mine/<日期-说明>/）。
#
# 用法：
#   ./lab.sh                                       交互菜单
#   ./lab.sh new    <科目> <关卡号> [说明]           新建尝试；说明缺省为 first、review、review2……
#   ./lab.sh new    lib 02-linked-list             关卡还不存在时给完整目录名，先建关卡骨架
#   ./lab.sh list   [科目] [关卡号]                  查看关卡、尝试和完成标记
#   ./lab.sh rename <科目> <关卡号> <尝试> <新说明>   只改说明，日期不变，程序名一起改
#   ./lab.sh delete <科目> <关卡号> <尝试>           确认后删除（NOTES.md 里的复盘保留）
#   ./lab.sh done   <科目> <关卡号> <尝试>           标记完成：依赖本关的尝试改用这次的代码
#   ./lab.sh undone <科目> <关卡号> <尝试>           取消完成标记
#   ./lab.sh sync   [科目] [关卡号]                  重新生成关卡配置、tests/check.cmake、各项目 deps/
#
# <尝试> 写完整目录名（如 2026-03-01-first）或 list 里显示的编号都可以。
# 科目：lib / co / os / net / example；说明只能用小写字母、数字和连字符，同一关内不能重复（会成为程序名）。
# 增删改之后，在 CLion 里重新加载 CMake 项目才能看到变化。

set -euo pipefail
trap 'exit 1' TERM

root="$(cd "$(dirname "$0")" && pwd)"
subjects=(lib co os net example)
subject_names=(数据结构 计组 操作系统 计网 样板)

die() { echo "错误：$*" >&2; exit 1; }

is_slug() { [[ "$1" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]]; }

check_subject() {
    case "$1" in
        lib|co|os|net|example) ;;
        *) die "科目只能是 lib、co、os、net、example，收到的是：$1" ;;
    esac
}

# 输出 subject 下以 num 开头的已有关卡目录（可能为空）
find_level_dir() {
    local d
    for d in "$root/$1/$2"-*; do
        if [ -d "$d" ]; then echo "$d"; fi
    done
    return 0
}

# 按名字（即日期）升序输出关卡的全部尝试名
attempts_of() {
    [ -d "$1/mine" ] || return 0
    find "$1/mine" -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort
}

count_attempts() { attempts_of "$1" | grep -c . || true; }

rel() { echo "${1#"$root"/}"; }

source "$root/cmake/lab-units.sh"

# 依赖本关的尝试实际会复制哪份代码，规则同 dep_src_dir
used_unit() {
    local a picked="answer/"
    for a in $(attempts_of "$1"); do
        if [ -f "$1/mine/$a/.done" ]; then picked="mine/$a"; fi
    done
    echo "$picked"
}

desc_of() { echo "${1#[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]-}"; }

desc_taken() {
    local a
    for a in $(attempts_of "$1"); do
        if [ "$(desc_of "$a")" = "$2" ]; then return 0; fi
    done
    return 1
}

default_desc() {
    local n=2
    if ! desc_taken "$1" first; then echo first; return; fi
    if ! desc_taken "$1" review; then echo review; return; fi
    while desc_taken "$1" "review$n"; do n=$((n + 1)); done
    echo "review$n"
}

project_name() { echo "$(level_base "$1")_${2//-/_}"; }

notes_header() {
    printf '# %s/%s 复盘\n\n## 概念卡片\n\n' "$1" "$2"
}

# ---------- 定位关卡与尝试（结果放进全局变量 level_dir、attempt） ----------

check_level_arg() {
    case "$1" in
        [0-9][0-9]) ;;
        [0-9][0-9]-*) is_slug "${1:3}" || die "关卡目录名只能用小写字母、数字和连字符：$1" ;;
        *) die "关卡号要写成两位数字（如 02），或完整目录名（如 02-linked-list）" ;;
    esac
}

# 关卡必须已存在
require_level() {
    local subject="$1" level_arg="$2" existing
    check_level_arg "$level_arg"
    existing="$(find_level_dir "$subject" "${level_arg:0:2}")"
    [ -n "$existing" ] || die "$subject/${level_arg:0:2} 还没建，先用 ./lab.sh new 新建"
    [ "$(echo "$existing" | wc -l)" -eq 1 ] || die "$subject 下有多个 ${level_arg:0:2} 开头的关卡目录，请先清理"
    level_dir="$existing"
}

# 新建时用：关卡不存在就按完整目录名建骨架
ensure_level() {
    local subject="$1" level_arg="$2" num="${2:0:2}"
    check_level_arg "$level_arg"
    if [ -n "$(find_level_dir "$subject" "$num")" ]; then
        require_level "$subject" "$level_arg"
        if [ "$level_arg" != "$num" ] && [ "$(basename "$level_dir")" != "$level_arg" ]; then
            die "$subject 下已有关卡 $(basename "$level_dir")，和你给的 ${level_arg} 不一致"
        fi
        return
    fi
    [ "$level_arg" != "$num" ] || die "$subject/$num 还不存在，第一次请给完整目录名，例如 ./lab.sh new $subject $num-xxx"
    level_dir="$root/$subject/$level_arg"
    mkdir -p "$level_dir/mine"
    notes_header "$subject" "$level_arg" > "$level_dir/NOTES.md"
    sync_level "$level_dir"
    echo "已创建关卡骨架：$subject/$level_arg/"
}

require_attempt() {
    local list n
    list="$(attempts_of "$level_dir")"
    [ -n "$list" ] || die "$(rel "$level_dir") 还没有任何尝试"
    if [[ "$1" =~ ^[0-9]+$ ]]; then
        n="$(echo "$list" | wc -l | tr -d ' ')"
        if [ "$1" -lt 1 ] || [ "$1" -gt "$n" ]; then die "编号 ${1} 超出范围（1-${n}）"; fi
        attempt="$(echo "$list" | sed -n "${1}p")"
    else
        grep -xF -- "$1" <<< "$list" >/dev/null || die "没有这次尝试：${1}（用 ./lab.sh list 查看）"
        attempt="$1"
    fi
}

# ---------- 各项操作（读全局 level_dir、attempt） ----------

do_new() {
    local desc="$1" name dir pname
    [ -n "$desc" ] || desc="$(default_desc "$level_dir")"
    is_slug "$desc" || die "说明只能用小写字母、数字和连字符，例如 recursive、two-pointer"
    ! desc_taken "$level_dir" "$desc" || die "本关已有说明为 ${desc} 的尝试，程序会重名，换一个说明"
    name="$(date +%F)-$desc"
    dir="$level_dir/mine/$name"
    pname="$(project_name "$level_dir" "$desc")"
    mkdir -p "$dir"
    create_project "$level_dir" "$dir" "$pname"
    sync_level "$level_dir"

    [ -f "$level_dir/NOTES.md" ] || notes_header "$(basename "$(dirname "$level_dir")")" "$(basename "$level_dir")" > "$level_dir/NOTES.md"
    printf '\n## %s\n- \n' "$name" >> "$level_dir/NOTES.md"

    echo "已创建项目：$(rel "$dir")/"
    (cd "$dir" && find . -type f ! -path './deps/*' | sort | sed 's|^\./|  - |')
    echo "  - deps/（$(ls "$dir/deps" | tr '\n' ' ')）"
    echo "已在 NOTES.md 末尾添加小节：## $name"
    echo "在 CLion 里重新加载 CMake 项目，运行目标里就会出现 ${pname}、${pname}_test；也可以单独用 CLion 打开这个文件夹。"
}

# 列出尝试；第二个参数为 files 时连文件一起列
show_attempts() {
    local i=0 a mark list
    list="$(attempts_of "$level_dir")"
    if [ -z "$list" ]; then
        echo "$(rel "$level_dir") 还没有尝试，用 ./lab.sh new 新建"
        return
    fi
    echo "$(rel "$level_dir") 的尝试："
    while read -r a; do
        i=$((i + 1))
        mark=""
        if [ -f "$level_dir/mine/$a/.done" ]; then mark="  [已完成]"; fi
        printf '  %d) %s%s\n' "$i" "$a" "$mark"
        if [ "${1:-}" = files ]; then
            (cd "$level_dir/mine/$a" && find . -type f ! -name .done ! -path './deps/*' ! -path './cmake-build-*' ! -path './.idea/*' | sort | sed 's|^\./|       |')
        fi
    done <<< "$list"
    echo "依赖本关的尝试当前使用：$(used_unit "$level_dir")"
}

do_rename() {
    local desc="$1" new notes tmp old_name new_name cml
    is_slug "$desc" || die "说明只能用小写字母、数字和连字符"
    [[ "$attempt" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}- ]] || die "${attempt} 不是「日期-说明」格式，请手动改名"
    new="${attempt:0:10}-$desc"
    [ "$new" != "$attempt" ] || die "新名字和原来一样"
    ! desc_taken "$level_dir" "$desc" || die "本关已有说明为 ${desc} 的尝试，程序会重名"
    mv "$level_dir/mine/$attempt" "$level_dir/mine/$new"
    old_name="$(project_name "$level_dir" "$(desc_of "$attempt")")"
    new_name="$(project_name "$level_dir" "$desc")"
    cml="$level_dir/mine/$new/CMakeLists.txt"
    if [ -f "$cml" ]; then sed -i '' "s/${old_name}/${new_name}/g" "$cml"; fi
    rm -rf "$level_dir/mine/$new"/cmake-build-*
    write_level_cmake "$level_dir"
    notes="$level_dir/NOTES.md"
    if [ -f "$notes" ]; then
        tmp="$(mktemp)"
        awk -v o="## $attempt" -v n="## $new" '$0 == o { $0 = n } 1' "$notes" > "$tmp"
        cat "$tmp" > "$notes"
        rm -f "$tmp"
    fi
    echo "已改名：mine/${attempt} → mine/${new}，程序名 ${old_name} → ${new_name}（NOTES.md 小节标题已同步）"
}

do_delete() {
    local dir="$level_dir/mine/$attempt" reply was_done=no
    if [ -f "$dir/.done" ]; then was_done=yes; fi
    echo "将删除 $(rel "$dir")/，其中的文件："
    (cd "$dir" && find . -type f ! -path './cmake-build-*' ! -path './.idea/*' | sort | sed 's|^\./|  - |')
    reply="$(ask "确认删除？NOTES.md 里的复盘会保留 (y/n)" "n")"
    case "$reply" in
        y|Y|yes)
            rm -rf "$dir"
            write_level_cmake "$level_dir"
            echo "已删除：mine/$attempt"
            if [ "$was_done" = yes ]; then sync_dependents "$level_dir"; fi ;;
        *) echo "已取消，没有删除任何文件" ;;
    esac
}

do_mark() {
    local mark="$level_dir/mine/$attempt/.done"
    if [ "$1" = done ]; then
        : > "$mark"
        echo "已标记完成：mine/$attempt"
    else
        rm -f "$mark"
        echo "已取消完成标记：mine/$attempt"
    fi
    echo "依赖本关的尝试现在使用：$(used_unit "$level_dir")"
    sync_dependents "$level_dir"
}

do_sync() {
    local d n=0
    for d in "$@"; do
        sync_level "$d"
        n=$((n + 1))
    done
    echo "已同步 ${n} 个关卡：关卡 CMakeLists.txt、tests/check.cmake、TASK.md 占位、各项目 deps/"
}

# ---------- 交互 ----------

# ask 总在 $(...) 子 shell 里运行，exit 只能退出子 shell，所以输入结束时直接结束主进程，
# 否则外层的「重新输入」循环会无限转下去
input_ended() {
    echo "错误：输入已结束，已退出" >&2
    kill -TERM $$
    exit 1
}

ask() {
    local prompt="$1" default="${2:-}" reply
    if [ -n "$default" ]; then
        read -r -p "${prompt} [${default}]：" reply || input_ended
        echo "${reply:-$default}"
    else
        read -r -p "${prompt}：" reply || input_ended
        echo "$reply"
    fi
}

# 反复询问直到输入是 1..max 的编号
ask_number() {
    local prompt="$1" max="$2" reply
    while true; do
        reply="$(ask "$prompt")" || exit 1
        if [[ "$reply" =~ ^[0-9]+$ ]] && [ "$reply" -ge 1 ] && [ "$reply" -le "$max" ]; then
            echo "$reply"
            return
        fi
        echo "「${reply}」不行，请输入 1 到 ${max}" >&2
    done
}

pick_subject() {
    local i n
    echo "选择科目："
    for i in "${!subjects[@]}"; do
        echo "  $((i + 1))) ${subjects[$i]}  ${subject_names[$i]}"
    done
    n="$(ask_number "输入编号" "${#subjects[@]}")"
    subject="${subjects[$((n - 1))]}"
}

level_status() {
    if [ -z "$1" ]; then echo "未建"; return; fi
    echo "尝试 $(count_attempts "$1") 次，依赖用 $(used_unit "$1")"
}

# 有 LEVELS.md 时按关卡总览表列出（含未建的关），否则列出已有目录
list_levels() {
    local s="$1" levels_md="$root/$1/LEVELS.md" num topic dir
    echo "$s 的关卡："
    if [ ! -f "$levels_md" ]; then
        for dir in "$root/$s"/[0-9][0-9]-*; do
            if [ -d "$dir" ]; then printf '  %s  [%s]\n' "$(basename "$dir")" "$(level_status "$dir")"; fi
        done
        return 0
    fi
    grep -E '^\| [0-9]{2} \|' "$levels_md" | awk -F'|' '!seen[$2]++ {gsub(/^ +| +$/, "", $2); gsub(/^ +| +$/, "", $3); print $2 "\t" $3}' |
    while IFS=$'\t' read -r num topic; do
        dir="$(find_level_dir "$s" "$num" | head -1)"
        printf '  %s  %s  [%s]\n' "$num" "$topic" "$(level_status "$dir")"
    done
}

ask_level_num() {
    local num
    while true; do
        num="$(ask "输入两位关卡号，例如 02")" || exit 1
        if [[ "$num" =~ ^[0-9]{2}$ ]]; then echo "$num"; return; fi
        echo "关卡号要写成两位数字" >&2
    done
}

pick_existing_level() {
    local num
    list_levels "$subject"
    while true; do
        num="$(ask_level_num)"
        if [ -n "$(find_level_dir "$subject" "$num")" ]; then break; fi
        echo "$subject/$num 还没建，换一个，或回到菜单选「新建」" >&2
    done
    require_level "$subject" "$num"
}

pick_attempt() {
    local n
    n="$(count_attempts "$level_dir")"
    [ "$n" -gt 0 ] || die "$(rel "$level_dir") 还没有任何尝试"
    show_attempts
    require_attempt "$(ask_number "选择尝试编号" "$n")"
}

ask_slug() {
    local reply
    while true; do
        reply="$(ask "$1" "${2:-}")" || exit 1
        if is_slug "$reply"; then echo "$reply"; return; fi
        echo "「${reply}」不行：只能用小写字母、数字和连字符，不能为空、不能有空格或中文" >&2
    done
}

interactive_new() {
    local num level_arg desc default=first reply
    list_levels "$subject"
    num="$(ask_level_num)"
    level_arg="$num"
    if [ -z "$(find_level_dir "$subject" "$num")" ]; then
        echo "这一关还没建，需要起一个英文目录名。"
        level_arg="$num-$(ask_slug "英文目录名，例如 linked-list")"
    else
        require_level "$subject" "$num"
        default="$(default_desc "$level_dir")"
        if [ "$(count_attempts "$level_dir")" -gt 0 ]; then show_attempts; fi
    fi
    desc="$(ask_slug "这次尝试的说明（如 review、recursive）" "$default")"
    reply="$(ask "将在 $subject/$level_arg 下创建 mine/$(date +%F)-${desc}，确认？(y/n)" "y")"
    case "$reply" in
        y|Y|yes) ;;
        *) echo "已取消"; exit 0 ;;
    esac
    ensure_level "$subject" "$level_arg"
    do_new "$desc"
}

interactive() {
    local action
    echo "要做什么："
    echo "  1) 新建尝试   2) 查看   3) 改名   4) 删除   5) 标记完成   6) 取消完成标记   7) 同步全部"
    action="$(ask_number "输入编号" 7)"
    if [ "$action" -eq 7 ]; then do_sync $(all_levels); return; fi
    pick_subject
    if [ "$action" -eq 1 ]; then interactive_new; return; fi
    pick_existing_level
    case "$action" in
        2) show_attempts files ;;
        3) pick_attempt; do_rename "$(ask_slug "新的说明（日期不变）")" ;;
        4) pick_attempt; do_delete ;;
        5) pick_attempt; do_mark done ;;
        6) pick_attempt; do_mark undone ;;
    esac
}

# ---------- 命令行 ----------

usage() {
    sed -n '4,13p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit "${1:-1}"
}

cmd_list() {
    local s
    case $# in
        0) for s in "${subjects[@]}"; do list_levels "$s"; echo; done ;;
        1) check_subject "$1"; list_levels "$1" ;;
        2) check_subject "$1"; require_level "$1" "$2"; show_attempts files ;;
        *) usage ;;
    esac
}

cmd_sync() {
    local d
    case $# in
        0) do_sync $(all_levels) ;;
        1) check_subject "$1"; do_sync $(for d in "$root/$1"/[0-9][0-9]-*; do if [ -d "$d" ]; then echo "$d"; fi; done) ;;
        2) check_subject "$1"; require_level "$1" "$2"; do_sync "$level_dir" ;;
        *) usage ;;
    esac
}

main() {
    local cmd
    subject="" level_dir="" attempt=""
    if [ $# -eq 0 ]; then interactive; return; fi
    cmd="$1"; shift
    case "$cmd" in
        list) cmd_list "$@"; return ;;
        sync) cmd_sync "$@"; return ;;
        new) if [ $# -lt 2 ] || [ $# -gt 3 ]; then usage; fi
             check_subject "$1"; ensure_level "$1" "$2"; do_new "${3:-}"; return ;;
        rename) [ $# -eq 4 ] || usage ;;
        delete|done|undone) [ $# -eq 3 ] || usage ;;
        -h|--help|help) usage 0 ;;
        *) echo "不认识的命令：$cmd" >&2; usage ;;
    esac
    check_subject "$1"
    require_level "$1" "$2"
    require_attempt "$3"
    case "$cmd" in
        rename) do_rename "$4" ;;
        delete) do_delete ;;
        done|undone) do_mark "$cmd" ;;
    esac
}

main "$@"
