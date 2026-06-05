


#define _GNU_SOURCE
#include <math.h>

#include "normalize.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "vector.h"

#define norm_max_amount 10000.0f
#define norm_max_installments 12.0f
#define norm_amount_vs_avg_ratio 10.0f
#define norm_max_minutes 1440.0f
#define norm_max_km 1000.0f
#define norm_max_tx_count_24h 20.0f
#define norm_max_merchant_avg_amount 10000.0f

static float round4(const float v) {
    return roundf(v * 10000.0f) / 10000.0f;
}

static float clamp01(const float v) {
    return v < 0 ? 0 : v > 1 ? 1 : v;
}


static int day_of_week(const int Y, const int M, const int D) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    const int y = (M < 3) ? Y - 1 : Y;
    const int dow = (y + y/4 - y/100 + y/400 + t[M-1] + D) % 7; 
    return (dow + 6) % 7; 
}

static float mcc_risk(const uint32_t mcc) {
    switch (mcc) {
        case 5411: return 0.15f;
        case 5812: return 0.30f;
        case 5912: return 0.20f;
        case 5944: return 0.45f;
        case 7801: return 0.80f;
        case 7802: return 0.75f;
        case 7995: return 0.85f;
        case 4511: return 0.35f;
        case 5999: return 0.50f;
        case 5311: return 0.25f;
        default: return 0.5f;
    }
}

static inline void parse_iso8601(const char *s,
                                 int *Y, int *M, int *D,
                                 int *H, int *Mi, int *S) {
    *Y  = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    *M  = (s[5]-'0')*10 + (s[6]-'0');
    *D  = (s[8]-'0')*10 + (s[9]-'0');
    *H  = (s[11]-'0')*10 + (s[12]-'0');
    *Mi = (s[14]-'0')*10 + (s[15]-'0');
    *S  = (s[17]-'0')*10 + (s[18]-'0');
}

static inline time_t ymdhms_to_epoch(int Y, int M, int D, int H, int Mi, int S) {
    static const int days_before_month[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    long yrs = Y - 1970;
    long leap_days = (yrs + 1)/4 - (yrs + 69)/100 + (yrs + 369)/400;
    long days = yrs * 365 + leap_days + days_before_month[M-1] + (D - 1);
    int is_leap = ((Y & 3) == 0 && (Y % 100 != 0 || Y % 400 == 0));
    if (is_leap && M > 2) days++;
    return days * 86400L + H*3600L + Mi*60L + S;
}

void normalize(const Request* req, vec_t out) {
    int y, mo, d, h, mi, s;
    parse_iso8601(req->requested_at, &y, &mo, &d, &h, &mi, &s);
    const time_t req_epoch = ymdhms_to_epoch(y, mo, d, h, mi, s);
    const int dow = day_of_week(y, mo, d);

    out[0]  = clamp01(req->amount / norm_max_amount);
    out[1]  = clamp01((float)req->installments / norm_max_installments);
    out[2]  = clamp01((req->amount / req->cust_avg) / norm_amount_vs_avg_ratio);
    out[3]  = (float)h / 23.0f;
    out[4]  = (float)dow / 6.0f;

    if (req->has_last) {
        int ly, lmo, ld, lh, lmi, ls;
        parse_iso8601(req->last_ts, &ly, &lmo, &ld, &lh, &lmi, &ls);
        const time_t last_epoch = ymdhms_to_epoch(ly, lmo, ld, lh, lmi, ls);
        const float mins = (float)(req_epoch - last_epoch) / 60.0f;
        out[5] = clamp01(mins / norm_max_minutes);
        out[6] = clamp01(req->last_km / norm_max_km);
    } else {
        out[5] = -1.0f;
        out[6] = -1.0f;
    }

    out[7]  = clamp01(req->km_home / norm_max_km);
    out[8]  = clamp01((float)req->tx_count_24h / norm_max_tx_count_24h);
    out[9]  = req->is_online ? 1.0f : 0.0f;
    out[10] = req->card_present ? 1.0f : 0.0f;

    int known = 0;
    for (int i = 0; i < req->known_n; i++)
        if (req->known[i] == req->merch_id) { known = 1; break; }
    out[11] = known ? 0.0f : 1.0f;

    out[12] = mcc_risk(req->mcc);
    out[13] = clamp01(req->merch_avg / norm_max_merchant_avg_amount);

    for (int i = 0; i < DIM; i++) {
        out[i] = round4(out[i]);
    }
}
