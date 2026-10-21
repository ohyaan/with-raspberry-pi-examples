// An exercise threshold, not a hardware safety limit.
const NOTICE_MILLIDEGREES: i32 = 60_000;

fn needs_notice(millidegrees: i32) -> bool {
    millidegrees >= NOTICE_MILLIDEGREES
}

fn main() {
    // Fixed data: this program does not read the Pi's sensors.
    let readings: [i32; 3] = [46_700, 60_000, 63_200];
    let mut notice_count: usize = 0;

    for millidegrees in readings {
        let celsius = f64::from(millidegrees) / 1_000.0;
        let notice = needs_notice(millidegrees);
        let label = if notice { "notice" } else { "below threshold" };

        if notice {
            notice_count += 1;
        }

        println!("{celsius:.1} C: {label}");
    }

    println!("Readings at or above the exercise threshold: {notice_count}");
}

#[cfg(test)]
mod tests {
    use super::needs_notice;

    #[test]
    fn below_threshold_does_not_need_notice() {
        assert!(!needs_notice(59_999));
    }

    #[test]
    fn threshold_itself_needs_notice() {
        assert!(needs_notice(60_000));
    }

    #[test]
    fn above_threshold_needs_notice() {
        assert!(needs_notice(60_001));
    }
}
