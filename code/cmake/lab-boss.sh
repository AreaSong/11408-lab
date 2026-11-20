# 由 lab.sh 引入（source），不单独运行：MiniMachine（boss）整机工程的阶段记录、借用子系统与从标签起步。
# 用到 lab.sh 里的 root、die、rel、ask、usage、require_level、require_attempt 以及全局 level_dir、attempt。
#
# 约定：
#   - 参考实现每发布一个阶段，由用户提交后打标签 boss-stage-NN（流程见 boss/AGENTS.md）
#   - 尝试目录的 .stage 记当前学到第几阶段（两位数字），tests/check.cmake 只登记到这一阶段的测试
#   - 尝试目录的 .borrowed 每行「<子系统> <标签>」；「* <标签>」表示整个尝试以该标签的参考实现为底

boss_last_stage=13

is_boss_level() { [ "$(basename "$(dirname "$level_dir")")" = boss ]; }

boss_tag() { echo "boss-stage-$1"; }

check_stage_num() {
    [[ "$1" =~ ^[0-9]{2}$ ]] || die "阶段号要写成两位数字（如 05），收到的是：$1"
    [ "$((10#$1))" -le "$boss_last_stage" ] || die "阶段号只能是 00~${boss_last_stage}，收到的是：$1"
}

# 尝试目录的 .stage；没有时输出空串
stage_of() {
    if [ -f "$1/.stage" ]; then tr -d '[:space:]' < "$1/.stage"; fi
}

# 标签不存在时说清楚原因：参考实现的这个阶段还没发布
require_stage_tag() {
    local tag have
    tag="$(boss_tag "$1")"
    git -C "$root" rev-parse --git-dir >/dev/null 2>&1 || die "$root 不在 git 仓库里，读不到参考实现的标签 ${tag}"
    if ! git -C "$root" rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
        have="$(git -C "$root" tag -l 'boss-stage-*' | tr '\n' ' ')"
        die "还没有标签 ${tag}：MiniMachine 参考实现的阶段 $1 还没发布。AI 写完一个阶段后由你提交，再打标签 ${tag}（或提交后对 AI 说「打标签」）。现有的阶段标签：${have:-无}"
    fi
}

# 标签里 answer/ 下的某个路径（空串表示整个 answer/）在仓库里的写法
boss_tag_path() { echo "$(git -C "$root" rev-parse --show-prefix)$(rel "$level_dir")/answer${1:+/$1}"; }

require_tag_path() {
    git -C "$root" cat-file -e "$(boss_tag "$1"):$(boss_tag_path "$2")" 2>/dev/null ||
        die "标签 $(boss_tag "$1") 里没有 $(rel "$level_dir")/answer${2:+/$2}（阶段 $1 的参考实现还没有这部分）"
}

# boss_extract <阶段号> <answer 下的路径> <临时目录>：把标签里这个目录的内容解到临时目录下的 x/，输出 x/ 的路径
# 用「标签:路径」只打包这棵子树；git archive 在子目录里运行会按当前目录过滤，所以从仓库根目录运行。
# $(...) 里出错不会让脚本退出，所以由调用方检查解出的内容
boss_extract() {
    local out="$3/x" top
    mkdir -p "$out"
    top="$(git -C "$root" rev-parse --show-toplevel)"
    git -C "$top" archive --format=tar "$(boss_tag "$1"):$(boss_tag_path "$2")" | tar -x -C "$out" || true
    echo "$out"
}

require_extracted() {
    [ -n "$(ls -A "$1")" ] || die "没能从标签 $(boss_tag "$2") 解出 $3，没有改动任何文件"
}

# ---------- new：写 .stage；--from-stage 以标签里的参考实现为底 ----------

# 在建任何文件之前检查并解出标签里的参考实现（放进 boss_from_dir），失败时不留下半个尝试
boss_from_dir=""
boss_check_new() {
    [ -f "$level_dir/answer/CMakeLists.txt" ] ||
        die "MiniMachine 的参考实现还没开始写（$(rel "$level_dir")/answer/ 不存在），等阶段 00 写完并打上标签 boss-stage-00 后再新建尝试"
    [ -n "$1" ] || return 0
    check_stage_num "$1"
    require_stage_tag "$1"
    require_tag_path "$1" ""
    boss_from_dir="$(boss_extract "$1" "" "$(mktemp -d)")"
    require_extracted "$boss_from_dir" "$1" "answer/"
}

boss_setup_attempt() {
    local dir="$1" from="$2" src="$boss_from_dir" f next
    if [ -z "$from" ]; then
        echo 00 > "$dir/.stage"
        return
    fi
    rm -rf "$dir/src"
    if [ -d "$src/src" ]; then cp -R "$src/src" "$dir/src"; else mkdir -p "$dir/src"; fi
    for f in "$src"/*.c; do
        if [ -f "$f" ]; then cp "$f" "$dir/"; fi
    done
    rm -rf "$(dirname "$src")"
    next=$((10#$from + 1))
    if [ "$next" -gt "$boss_last_stage" ]; then next="$boss_last_stage"; fi
    printf '%02d\n' "$next" > "$dir/.stage"
    echo "* $(boss_tag "$from")" > "$dir/.borrowed"
    echo "已用 $(boss_tag "$from") 的参考实现作底（src/ 与入口文件），.stage 设为 $(stage_of "$dir")"
}

# ---------- borrow：从已发布阶段的标签借一个子系统 ----------

# 借用会整个替换 src/<子系统>/，里面有文件时先确认
boss_confirm_overwrite() {
    local reply
    if [ ! -d "$1" ] || [ -z "$(find "$1" -type f)" ]; then return 0; fi
    echo "$(rel "$1")/ 里已有文件，借用会整个替换它们："
    (cd "$1" && find . -type f | sort | sed 's|^\./|  - |')
    reply="$(ask "确认替换？(y/n)" "n")"
    case "$reply" in
        y|Y|yes) ;;
        *) echo "已取消，没有改动任何文件"; exit 0 ;;
    esac
}

boss_record_borrow() {
    local file="$1/.borrowed" tmp
    tmp="$(mktemp)"
    if [ -f "$file" ]; then awk -v s="$2" '$1 != s' "$file" > "$tmp"; fi
    echo "$2 $3" >> "$tmp"
    cat "$tmp" > "$file"
    rm -f "$tmp"
}

# 缺省借 .stage 的上一阶段；只能借比当前阶段早的，借当前或更后的阶段等于直接看答案
boss_borrow_stage() {
    local cur="$1" want="$2"
    if [ -z "$want" ]; then
        [ "$cur" != 00 ] || die "当前在阶段 00，没有更早的阶段可借"
        want="$(printf '%02d' $((10#$cur - 1)))"
    fi
    check_stage_num "$want"
    [ "$((10#$want))" -lt "$((10#$cur))" ] ||
        die "只能借比当前阶段 ${cur} 更早的阶段，收到的是 ${want}（借当前或更后的阶段等于直接看答案）"
    echo "$want"
}

cmd_borrow() {
    local sub dir cur stage tmp src
    [ $# -eq 3 ] || [ $# -eq 4 ] || usage
    [ "$1" = boss ] || die "borrow 只用于 boss（MiniMachine 整机工程），收到的科目是：$1"
    require_level boss 00
    require_attempt "$2"
    sub="$3"
    dir="$level_dir/mine/$attempt"
    is_slug "$sub" || die "子系统名只能用小写字母、数字和连字符，如 cpu、mem、kernel"
    cur="$(stage_of "$dir")"
    [ -n "$cur" ] || die "mine/$attempt 没有 .stage，先写入当前阶段号（如 echo 05 > .stage）"
    check_stage_num "$cur"
    stage="$(boss_borrow_stage "$cur" "${4:-}")"
    require_stage_tag "$stage"
    require_tag_path "$stage" "src/$sub"
    boss_confirm_overwrite "$dir/src/$sub"
    tmp="$(mktemp -d)"
    src="$(boss_extract "$stage" "src/$sub" "$tmp")"
    require_extracted "$src" "$stage" "answer/src/$sub/"
    mkdir -p "$dir/src"
    rm -rf "$dir/src/$sub"
    cp -R "$src" "$dir/src/$sub"
    rm -rf "$tmp"
    boss_record_borrow "$dir" "$sub" "$(boss_tag "$stage")"
    echo "已从 $(boss_tag "$stage") 借入 src/$sub/ 到 mine/${attempt}（记在 .borrowed），在 CLion 里重新加载 CMake 项目"
}

# list 里每次尝试后面的「阶段、借用」说明；没有 .stage 的（非 boss）输出空串
boss_attempt_note() {
    local st borrowed=""
    st="$(stage_of "$1")"
    [ -n "$st" ] || return 0
    if [ -s "$1/.borrowed" ]; then
        borrowed="，借用：$(awk '{ printf "%s%s←%s", (NR > 1 ? " " : ""), ($1 == "*" ? "整机" : $1), $2 }' "$1/.borrowed")"
    fi
    printf '  [阶段 %s%s]' "$st" "$borrowed"
}
