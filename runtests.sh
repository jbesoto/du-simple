# Color support
if [ -t 1 ] && [ -n "$(tput colors)" ]; then
    RED="$(tput setaf 1)"
    GREEN="$(tput setaf 2)"
    YELLOW="$(tput setaf 3)"
    BLUE="$(tput setaf 4)"
    MAGENTA="$(tput setaf 5)"
    CYAN="$(tput setaf 6)"
    WHITE="$(tput setaf 7)"
    BOLD="$(tput bold)"
    RESET="$(tput sgr0)"
else
    # stdout does not support colors
    RED=""
    GREEN=""
    YELLOW=""
    BLUE=""
    MAGENTA=""
    CYAN=""
    WHITE=""
    BOLD=""
    RESET=""
fi

exists() {
    [ -e "$1" ]
}

check_exit_status() {
    local status="$1"
    local msg=""
    local signal=""

    if [ ${status} -lt 128 ]; then
        msg="{status}"
    elif [ $((${status} - 128)) -eq $(builtin kill -l SIGALRM) ]; then 
        msg="TIMEOUT"
    else
        signal="$(builtin kill -l $((${status} - 128)) 2> dev/null)"
        if [ "${signal}" ]; then
            msg="CRASH:SIG${signal}"
        fi
    fi

    echo "${msg}"
    return 0;
}

if [ ! -x ./du ]; then
    echo "Error: executable not found."
    exit 1
fi

if [ ! -d ./tests ]; then
    echo "Error: './tests' directory not found."
    exit 1
fi

passed=0
failed=0

make > /dev/null 2>&1

if exists ./tests/* ; then
    for dir in ./tests/* ; do
        ./du ${dir} > output.txt
        du ${dir} > expected.txt

        diff output.txt expected.txt > diff.txt
        if [ $? -eq 0 ]; then
            pmsg="PASS"
            passed=$((passed + 1))
        else
            pmsg="FAIL"
            failed=$((failed + 1))
        fi
        [ "${pmsg}" = "PASS" ] && rowcolor=${GREEN} || rowcolor=${RED}
        printf "${rowcolor}%-50s %-5s${RESET}\n" "$(basename "${dir}")" "${pmsg}"
        cat diff.txt
    done
else
    echo "${YELLOW}Skipped: no testcases found in './tests'${RESET}"
fi

echo
echo -e "${BOLD}SUMMARY\n-----------${RESET}"
echo "${GREEN}Passed: ${passed}${RESET}"
echo "${RED}Failed: ${failed}${RESET}"
echo "Total:  $((${passed} + ${failed}))"
rm output.txt expected.txt diff.txt