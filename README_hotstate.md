## Hotstate: Runtime Loadable Microcoded Algorithmic State Machine (RLMASM)
Since c_parser intends to duplicate and extend hotstate this is deep dive into the hotsate code from hotwright.com

### What Hotstate Does

**Hotstate** is a specialized compiler that converts C code into **hardware state machines** for FPGA implementation.

### Key Characteristics

#### 1. **Hardware-Oriented C Compiler**
- Takes C code as input (like c_parser)
- But instead of generating CFGs for analysis, it generates **microcode for state machines**
- Specifically designed for FPGA hardware synthesis

#### 2. **State Machine Generation**
From the documentation, it converts C code like:
```c
bool LED0 = 0; /* state0 */
bool LED1 = 0; /* state1 */
bool LED2 = 1; /* state2 */

int main() {
    while(1) {
        if(a0 == 0 && a1 == 1)
            LED0 = 1, LED0 = 0;
        // ... more logic
    }
}
```

Into **microcode state machine tables**:
```
State Machine Microcode derived from simple.c 
              s s                
              w w s     f        
a             i i t t   o        
d       v t   t t a i b r        
d s     a i t c c t m r c        
r t m j r m i h h e / a e        
e a a a S S m s a C v n j s r    
s t s d e e L e d a a c m u t    
s e k r l l d l r p r h p b n    
---------------------------------
0 4 7 0 0 x x x x 1 0 0 0 0 0   main(){
1 0 0 e 0 x x x x 0 0 1 0 0 0   while (1) {
2 0 0 5 1 x x x x 0 0 1 0 0 0   if ((a0 == 0) && (a1 == 1)) {
...
```

#### 3. **FPGA-Specific Features**
- **State Variables**: Boolean variables become hardware states
- **Input Variables**: Hardware inputs (a0, a1, a2)
- **One-Hot Encoding**: Each state corresponds to an output (LED0, LED1, LED2)
- **Microcode Output**: Generates hardware description suitable for FPGA synthesis

### Comparison with c_parser

| Feature | Your c_parser | hotstate |
|---------|------------------------|----------|
| **Purpose** | C code analysis & CFG generation | C-to-hardware state machine compiler |
| **Input** | C source code | C source code (hardware-oriented) |
| **Output** | Abstract Syntax Tree + Control Flow Graph | Microcode state machine tables |
| **Target** | Software analysis/visualization | FPGA hardware synthesis |
| **Parser Type** | Hand-written recursive descent | Yacc/Bison grammar-based |
| **Focus** | Language features & program flow | Hardware state transitions |

### Technical Architecture of hotstate

#### Parser Implementation
- Uses **Yacc/Bison** grammar (`gram.y`) + **Flex** lexer (`scan.l`)
- More traditional compiler construction approach vs. your hand-written parser
- Includes specialized hardware constructs (T_STATE type)

#### Code Generation
- Generates **Verilog testbenches** (`make tb`)
- Creates **DOT files** for visualization (similar to your project)
- Outputs **microcode tables** for state machine implementation

### Unique Aspects

#### 1. **Hardware Semantics**
- Boolean variables become physical hardware states
- Assignments translate to state transitions
- Control flow becomes state machine logic

#### 2. **Real-Time Constraints**
- Designed for continuous operation (`while(1)` loops)
- State transitions happen on clock cycles
- Hardware-appropriate timing considerations

#### 3. **FPGA Integration**
- Generates synthesis-ready output
- Includes testbench generation
- Supports hardware debugging

### Usage Pattern
```bash
$ hotstate simple.c          # Parse and analyze
$ make alz                   # Generate microcode
$ make tb                    # Generate testbench
```

### Conclusion

**Hotstate** is essentially a **C-to-hardware compiler** that shares the front-end parsing challenge with your project but serves a completely different domain. While your c_parser focuses on **software analysis and visualization**, hotstate focuses on **hardware synthesis and state machine generation**.

Both projects demonstrate different approaches to C parsing:
- **Your approach**: Hand-written, focused on modern C features, educational/analytical
- **Hotstate approach**: Grammar-based, hardware-oriented, production-focused for FPGA development

The hotstate project shows how C parsing can be applied to hardware design, making it a fascinating parallel to your software-focused parser. It's particularly interesting that both projects generate DOT files for visualization, but hotstate's ultimate goal is hardware synthesis rather than program analysis.


See docs/hotstate_code_overview.md for a deep dive into the hotstate codebase.