#include <bits/stdc++.h>

using namespace std;

// Helper to construct constant string representations using only 'n'
string get_one() {
    return "(n/n)";
}

string get_const(int c) {
    if (c == 0) return "(n-n)";
    string one = get_one();
    string res = "(" + one;
    for (int i = 1; i < c; ++i) {
        res += "+" + one;
    }
    res += ")";
    return res;
}

// Generates the DC expression for a given k
string generate_dc_expression(int k) {
    if (k == 2 || k == 3) {
        // n - n/n yields n - 1, which evaluates to:
        // n = 2 -> 1 (!2 = 1)
        // n = 3 -> 2 (!3 = 2)
        return "n-n/n";
    }

    // For k = 50, we use: round(n! / e)
    // using Horner's method expansion for n! * sum_{j=2}^M (-1)^j / j!
    // M = 52 gives sufficient precision for all n <= 50.
    int M = 52;
    string inner = get_const(M);

    for (int j = M - 1; j >= 2; --j) {
        string sign = ((j % 2 == 0) ? "+" : "-");
        inner = "(" + get_one() + "/" + get_const(j) + sign + "(" + get_one() + "/(" + inner + ")))";
    }

    // Multiply by n * (n - 1) and enclose in round()
    string expr = "round(n*(n-n/n)*" + inner + ")";
    return expr;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    int k;
    if (!(cin >> k)) return 0;

    cout << generate_dc_expression(k) << "\n";

    return 0;
}