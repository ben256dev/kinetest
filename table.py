import sys
import json
from tabulate import tabulate

data = json.load(sys.stdin)

if isinstance(data, list):
    if data and isinstance(data[0], dict):
        print(tabulate(data, headers="keys", tablefmt="fancy_grid"))
    else:
        print(tabulate(data, tablefmt="fancy_grid"))
elif isinstance(data, dict):
    print(tabulate(data.items(), headers=["key", "value"], tablefmt="fancy_grid"))
else:
    print(tabulate([[data]], tablefmt="fancy_grid"))
