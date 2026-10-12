## Coding Rules

* Folders are named in **snake_case**
* Files are named in **PascalCase**
* Classes and Namespaces are also defined in **PascalCase**
* Utility types (math types like vector3df or custom smart pointers) are **snake_case** (just as the STL)
* Functions are defined in **lowerCamelCase**
* Class Member Variables are **m_lowerCamelCase**
* Static Class Member Variables **are sm_lowerCamelCase**
* Namespace variables are **ng_lowerCamelCase** (namespace global)
* Global Variables are **g_lowerCamelCase**
* #define constants are **ALL_CAPS** (Without prefix) 
* Public fields of passive data structs (DTOs/PODs and snake_case types) **do not use** the *m_* prefix.
* Enum types are PascalCase and their values are **ALL_CAPS** but must have a prefix usually consisting of the types first letters (e.g. RenderSystemType -> RST_BLA)
* Local variables should have no prefix. All prefix rules only count for non-local (global, memeber, etc...) variables!
* All Variables/Classes/Namespaces/Documentation and whatever in English (Or at least understandable for an english speaker) ;)
* You may add a space between if/for/while... and their () (e.g. `if (bla)`)
* Add the { after the if and function defs (K&R). And do NOT put it into the next line (e.g. `if (bla) {`)
* Short one-line *if*s do not require a curly brace 
* `else` and `else if` are written on the same line as the preceding `}` (K&R)
* Keep **function headers** in one *single line* even if they contain many parameters. (In headers AND implementation)
* Use classic one-line docs for functions.
* Use static classes if possible (to avoid singletons and unnecessary instances). E.g. for managers (SceneManager, GUIManager, AudioEngine...)
* **Unnamed namespaces** are forbidden
* Have a sense of humor ;)

### Variable Prefixes
| Prefix  | Meaning                                    | Example              | Description |
|---------|--------------------------------------------|----------------------|-------------|
| `x`     | **Constant at compile-time**               | `x_maxValue`        | Defined using `constexpr`, evaluated at compile-time. |
| `c`     | **Constant at runtime**                    | `c_configValue`     | Immutable but assigned at runtime (`const`). |
| `s`     | **Static variable**                        | `s_counter`         | A **static** variable is shared across all instances of a class. <br>  - **Static class**: `GUIManager::addWidget(widget);` <br>  - vs. **Instanced class/pointer**: `guiManagerInstance->addWidget(widget);` |
| `m`     | **Member variable** (instance variable)    | `m_testVariable`    | Belongs to a specific instance of a class. |
| `g`     | **Global variable**                        | `g_appVersion`      | Accessible across multiple files/modules. |
| `t`     | **Thread-local variable**                  | `t_threadID`        | Exists only within a single thread (`thread_local`). |

---

#### **Combining Prefixes**
Prefixes can be combined to describe multiple properties. For example:

| Combination | Meaning                                     | Example               |
|-------------|---------------------------------------------|-----------------------|
| `sm`        | **Static member variable**                  | `sm_testVariable`     |
| `xm`        | **Compile-time constant member variable**   | `xm_defaultValue`     |
| `smt`       | **Static and thread local member variable** | `smt_activeScriptID`  |
| `xg`        | **Compile-time constant global variable**   | `xg_defaultSize`      |
| `sg`        | **Static global variable**                  | `sg_counter`          |

---

> These rules are not a suggestion. They are sacred laws. There shall be no discussions about it!