# C Interactive Function Plotter & Excel Exporter

Et lettvekts, frittstående Windows-program skrevet i ren C (Win32 API & GDI) for interaktiv plotting og analyse av matematiske funksjoner. Ingen eksterne biblioteker kreves.

<img width="1441" height="996" alt="image" src="https://github.com/user-attachments/assets/e715640e-64e5-4b1a-bf17-46bde94a5799" />

## Funksjoner

- **Matematisk uttrykkstolker ($f(x)$)**:
  - Innebygd recursive-descent parser med operatorpresedens og implisitt multiplikasjon (f.eks. `2x`, `3sin(x)`, `(x+1)(x-1)`).
  - Operatorer: `+`, `-`, `*`, `/`, `^` (potens).
  - Funksjoner: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `sqrt`, `cbrt`, `exp`, `ln`, `log10`, `abs`, `floor`, `ceil`.
  - Konstanter: `pi`, `e`.
  - Asymptote- og pol-håndtering (unngår vertikale feillinjer ved diskontinuiteter som $\tan(x)$ eller $1/x$).

- **Zoom og navigasjon**:
  - **Zoom kun X-akse**: Knappene `[X +]` / `[X -]`, tastene `X` / `Shift+X`, eller **`Shift` + rullehjul**.
  - **Zoom kun Y-akse**: Knappene `[Y +]` / `[Y -]`, tastene `Y` / `Shift+Y`, eller **`Ctrl` + rullehjul**.
  - **Zoom begge samtidig**: Knappene `[Begge +]` / `[Begge -]`, tastene `+` / `-`, eller standard **rullehjul**.
  - **Panorering (dra grafen)**: Klikk og dra med venstre musetast, eller bruk piltastene.
  - **Nullstill**: Knapp `[Nullstill visning]` eller tast `R` for å gjenopprette standard $[-10, 10]$ utsnitt.

- **Tilpassbare akseetiketter og tittel**:
  - Egne tekstfelt for **Tittel**, **X-akseetikett** og **Y-akseetikett**.
  - Tittel vises sentrert i toppen i fet skrift.
  - X-aksetekst vises under aksen.
  - Y-aksetekst vises vertikalt ($90^\circ$ rotert) langs aksen.
  - Trykk `Enter` i ethvert tekstfelt for å oppdatere grafen umiddelbart.

- **Eksport til Excel med innebygd bilde (.xlsx)**:
  - Egen knapp **`[Eksporter til Excel (.xlsx)]`** eller hurtigtast **`Ctrl + S`**.
  - Åpner standard Windows lagre-dialog.
  - Genererer en ekte OpenXML Excel-arbeidsbok (`.xlsx`) med:
    1. **Datatabell:** 1000 nøyaktig beregnede $(x, f(x))$-punkter over gjeldende utsnitt.
    2. **Metadata:** Tittel, funksjonsuttrykk, definisjonsmengde og verdimengde.
    3. **Innebygd grafbilde:** Høyoppløselig bilde av funksjonsplottet plassert direkte ved siden av datatabellen.
  - Spør automatisk om du vil åpne filen direkte i Excel etter eksport.

## Kompilering og kjøring

### Med GCC (MinGW):
```bash
gcc -O2 -Wall -Wextra main.c -o plotter.exe -lgdi32 -luser32 -lcomdlg32 -lshell32 -mwindows
./plotter.exe
```

Eller kjør `build.bat`.
