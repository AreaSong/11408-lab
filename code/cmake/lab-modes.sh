# 由 lab.sh 引入（source），不单独运行：教学方式（--mode）与练习形式（--drill），见 code/TEACHING.md。
# 用到 lab.sh 里的 die、desc_taken、desc_of。
#
# 约定：
#   - 教学方式记在尝试目录的 .mode 文件里（内容如 m0）
#   - 练习形式 = 说明的后缀（唯一依据），如 first-blank、review-variant

modes=(m0 m1 m2 m3)
mode_names=("M0 领读（零基础手把手）" "M1 引导" "M2 项目指导" "M3 独立挑战")
drills=(blank bug rewrite variant)
drill_names=("挖空" "找 bug" "空白重写" "变式")

check_mode() {
    case "$1" in m0|m1|m2|m3) ;; *) die "--mode 只能是 m0、m1、m2、m3，收到的是：$1" ;; esac
}

check_drill() {
    case "$1" in none|blank|bug|rewrite|variant) ;; *) die "--drill 只能是 none、blank、bug、rewrite、variant，收到的是：$1" ;; esac
}

mode_label() {
    local i
    for i in "${!modes[@]}"; do
        if [ "${modes[$i]}" = "$1" ]; then echo "${mode_names[$i]}"; return; fi
    done
    echo "未记录"
}

# 空串表示正常写
drill_label() {
    local i
    for i in "${!drills[@]}"; do
        if [ "${drills[$i]}" = "$1" ]; then echo "${drill_names[$i]}"; return; fi
    done
    echo "正常写"
}

# 从说明（或「日期-说明」）的后缀识别练习形式；不是练习时输出空串
drill_of() {
    local d
    for d in "${drills[@]}"; do
        case "$1" in "$d"|*-"$d") echo "$d"; return ;; esac
    done
    echo ""
}

# 「方式 · 练习」标签，list 和 NOTES 用
attempt_label() {
    local dir="$1" mode=""
    if [ -f "$dir/.mode" ]; then mode="$(tr -d '[:space:]' < "$dir/.mode")"; fi
    echo "方式：$(mode_label "$mode") · 练习：$(drill_label "$(drill_of "$(basename "$dir")")")"
}

# 默认说明的基名（first、review、review2……）是否已被占用：带不带练习后缀都算
base_taken() {
    local d
    if desc_taken "$1" "$2"; then return 0; fi
    for d in "${drills[@]}"; do
        if desc_taken "$1" "$2-$d"; then return 0; fi
    done
    return 1
}

# 解出这次的说明：给了 --drill 而说明没带后缀就补上；没给 --drill 就按后缀识别
# 输出两行：说明、练习形式
resolve_desc_drill() {
    local desc="$1" drill="${2/#none/}"
    if [ -n "$drill" ]; then
        if [ -n "$desc" ] && [ "$(drill_of "$desc")" != "$drill" ]; then
            [ -z "$(drill_of "$desc")" ] || die "说明 ${desc} 的后缀和 --drill ${drill} 不一致"
            desc="$desc-$drill"
        fi
    else
        drill="$(drill_of "$desc")"
    fi
    printf '%s\n%s\n' "$desc" "$drill"
}

# 建好项目后写入模式文件；变式题再放一个空的 VARIANT.md 给 AI 写题面
write_mode_files() {
    local dir="$1" mode="$2" drill="$3"
    echo "$mode" > "$dir/.mode"
    if [ "$drill" = variant ]; then
        printf '# 变式题\n\n> 由 AI 按 code/TEACHING.md 填写：改了原题的什么条件、输入输出格式、样例；测试数据放在本目录的 tests/。\n' > "$dir/VARIANT.md"
    fi
}

# 建好后提示下一步该做什么
next_step_hint() {
    local drill="$1" target="$2"
    case "$drill" in
        blank) echo "下一步：跟 AI 说「填练习代码 ${target}」，AI 会在这次尝试里写好挖空骨架，关键行留给你填。" ;;
        bug) echo "下一步：跟 AI 说「填练习代码 ${target}」，AI 会在这次尝试里写一份埋了 bug 的代码，由你找出并修好（这次尝试不能 done）。" ;;
        variant) echo "下一步：跟 AI 说「出变式题 ${target}」，AI 会写 VARIANT.md 和本目录的 tests/，代码由你写（只跑本目录的测试，不能 done）。" ;;
        rewrite) echo "下一步：不看任何资料，限时重写；写完再和上次的尝试对比。" ;;
        *) ;;
    esac
}

# bug 和 variant 的代码不能给依赖本关的关卡复用
check_can_done() {
    case "$(drill_of "$1")" in
        bug) die "找 bug 练习（mine/${1}）不能标记完成：代码主体是 AI 生成的" ;;
        variant) die "变式题（mine/${1}）不能标记完成：它解的是另一道题，不能给依赖本关的关卡复用" ;;
        *) ;;
    esac
}

pick_mode() {
    local i n
    echo "选择教学方式（见 TEACHING.md）：" >&2
    for i in "${!modes[@]}"; do
        echo "  $((i + 1))) ${mode_names[$i]}" >&2
    done
    n="$(ask_number "输入编号" "${#modes[@]}" 2)"
    echo "${modes[$((n - 1))]}"
}

pick_drill() {
    local default_idx=1 i n
    if [ "$1" = m0 ]; then default_idx=2; fi
    echo "选择练习形式：" >&2
    echo "  1) 正常写（自己从头实现）" >&2
    for i in "${!drills[@]}"; do
        echo "  $((i + 2))) ${drill_names[$i]}（-${drills[$i]}）" >&2
    done
    n="$(ask_number "输入编号" "$(( ${#drills[@]} + 1 ))" "$default_idx")"
    if [ "$n" -eq 1 ]; then echo ""; else echo "${drills[$((n - 2))]}"; fi
}
