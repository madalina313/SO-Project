# City Manager — SO Project Phase 1

A C program built for the Unix environment that implements a
city infrastructure issue reporting and monitoring system.

## Features
- Add, list, view, filter, and remove infrastructure reports
- Binary file I/O using Unix system calls (open, read, write, lseek, ftruncate)
- Permission management with chmod() and stat()
- Role-based access control (manager / inspector)
- Operation logging in logged_district
- Symbolic links for active reports with dangling link detection

## How to Build
```bash
make
```

## How to Run
```bash
# Add a report
./city_manager --role manager --user admin --add downtown

# List reports
./city_manager --role manager --user admin --list downtown

# View a specific report
./city_manager --role manager --user admin --view downtown --id <ID>

# Filter reports
./city_manager --role inspector --user alice --filter downtown --condition "severity>=2"

# Remove a report (manager only)
./city_manager --role manager --user admin --remove_report downtown --id <ID>

# Update threshold (manager only)
./city_manager --role manager --user admin --update_threshold downtown --value 3
```

## Author
Termure Madalina — Politehnica Timișoara, CTI Year 2
Systems Programming (SO) — 2025/2026