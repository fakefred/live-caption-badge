import requests
import csv


def read_csv():
    badges = {}  # IP -> {key -> value}
    ip_addrs = []
    with open("dashboard.csv") as csv_file:
        csv_reader = csv.reader(csv_file)
        for row in csv_reader:
            row = list(row)
            if row[0] == "IP":
                ip_addrs = [row[1], row[2]]
                badges[row[1]] = dict()
                badges[row[2]] = dict()
            else:
                key = row[0].lower()
                badges[ip_addrs[0]][key] = row[1]
                badges[ip_addrs[1]][key] = row[2]
        csv_file.close()

    return badges


def post_user(ip, user):
    url = f"http://{ip}/user"
    try:
        requests.post(
            url,
            data="{};{};{};{}".format(
                user["name"], user["pronouns"], user["affiliation"], user["role"]
            ),
            timeout=5,
        )
    except requests.exceptions.Timeout:
        print(f"POST {url} timed out.")


def get(ip, endpoint):
    url = f"http://{ip}/{endpoint}"
    try:
        requests.get(
            url,
            timeout=5,
        )
    except requests.exceptions.Timeout:
        print(f"GET {url} timed out.")


def select_badge(badges):
    ip_addrs = []
    for idx, (ip, user) in enumerate(badges.items()):
        print(f"[{idx + 1}] {ip} {user['name']}")
        ip_addrs.append(ip)

    num_str = input("Select badge #: ")
    return ip_addrs[int(num_str) - 1]


if __name__ == "__main__":
    badges = read_csv()
    while True:
        cmd = input("[D]isplay new user / [R]eboot / [P]oke / [U]npoke: ").lower()
        if cmd == "d":
            new_badges = read_csv()
            found_changes = False
            for ip, user in new_badges.items():
                if user != badges[ip]:
                    print(f"{ip} user info has changed")
                    found_changes = True
                    post_user(ip, user)
                    badges = new_badges
            if not found_changes:
                print("No changes")
        else:
            try:
                ip = select_badge(badges)
            except IndexError:
                print("Invalid input")
                continue
            except ValueError:
                print("Invalid input")
                continue

            if cmd == "r":
                get(ip, "reboot")
            elif cmd == "p":
                get(ip, "poke")
            elif cmd == "u":
                get(ip, "unpoke")
