# pyCanDb

Wrapper for CanDb json description file.

Currently lacks intelligent way of field indexing. It's kinda pain...

```python

from candb import CanDB

db = CanDB(args.json_files)

#code from ocarina sniffer
db.parseData(ev.id.value, ev.data, ev.timestamp)
```