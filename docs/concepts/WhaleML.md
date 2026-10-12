# WhaleML - Orka's native Markup Language (for SceneNodes…)
Some compact native tree/list markup format without unnecessary chars.

### Example:
```WhaleML
// C++ style comments allowed

include "other_file.whaleml" // Will "paste" the file here (expand). You can override fields already defined in it

number : 1 // Key = value (can be key:value without whitespaces as well) with integral Nummer
float_number : 2.5 // Key = value with floating point number
some_string : "bla" // Key = value with string (can be 'bla' as well)
list : 1, 2, 3, 4, 5, 6 // List with a single type
multi_list : 1, 2.5, 3.666, "blah", 'chars', ["list inside another list", 2.5, 567]

// multi key=values in one line
number : 1; test : "bla"; whatever : 2.5

// Predefined value-type key = value (Special feature) (Like C++ ENUMS)
physics_body_type : rigid  // rigid = predefined value!

// You may use = instead of :

// Trees:
// I know they aren’t that powerful but at least they exist!
parent_object.child1 : 10
parent_object.child2 : 100
parent_object.child2.bla : 42

// Anti-redundant tree spelling. Supports tabs or 4 spaces
parent_object : 40
    child1 : 10
    child2 : 20
    child3 : 30
        subchild : 40
        subchild2 : 40
            grandsubchild : 20
    child4 : 5

// Edge cases
parent_object.test : 40
    foo : 30 // Foo will be child of test!


```

### Definitions:
- key: any word (no whitespace between!) might contain _ or -
- value: integral, float, bool (true/false), string ("" and ' ')
- single word string values (not containing any whitespaces) don't need quotes (e.g. body : rigid)
- classic: `key : val`
- list: `key : val1, val2, val3` (value types might differ)
- key/value separation: new line or ;
- key -> value: `:` and `=`
- list inside another list: `list : val1, [val1, val2, ...]`
- trees as parent.child.value (a child may also be a parent!) or tab based
- values (including lists) may have children
- Please keep the style consistent, ideally across the whole project, but at least within each file. Future-you will hate past-you otherwise ;/

### File extensions
- .whaleml
- (.whalemlbin)

### There will be a binary version as well (to increase parsing speed)... In the future... probably..... not......


# WhaleML Speed
Fresh test after the `WhaleMLFile` refactor, using C++23 `-O3` on Linux with an Intel Core i9-13900K.

Test data:
- 1000 player entries
- one 1000-item list
- 3000 operations per phase
- `loadBuffer()` reparses the full generated document every time
- Important: `15338.283 ms` is the combined time for 3000 full parses of a chunky generated test file with names, levels, booleans, and a fat list, not one tiny `window.width : 1280` config file making WhaleML faint lol. One parse averaged about `5.113 ms`.

| Operation | Total for 3000 ops | Average per op |
| --------- | ------------------ | -------------- |
| `loadBuffer()` | 15338.283 ms 😵🐢 | 5112.761 us 🐢 |
| `getValue()` | 0.472 ms ⚡ | 0.157 us ⚡ |
| `getValues()` | 15.653 ms 🚀 | 5.218 us 🚀 |
| `setValue()` | 0.688 ms ⚡ | 0.229 us ⚡ |
| `dump()` | 880.646 ms 🐌 | 293.549 us 🐌 |

That scary `15.338 s` number is 3000 full document parses stacked together. A single parse of this test document averaged `5.113 ms`, which is fine for loading/config paths but still not a per-frame thing.

`getValue()` and `setValue()` are the hot-path operations. `loadBuffer()` and `dump()` are full-document operations, so they should stay on loading/saving paths instead of per-frame logic! Do NOT try dumping 1000 times a second!
