#include "../localTypes.h"
#include "../inc/dayForth_i.h"

/* dayForth
 * This is the culmination of years of work on forth systems. I try to pull out the
 * best aspects of forth within the context of readable C/ASM. JonesForth is still a
 * ground breaking account of the forth architecture. This takes a different approach
 * with a lot being done in C for the language and leaving out the typical extension features
 * on purpose. They complicate the intricate machinery of the interpreter itself
 * which is a work of art in simplicity. A different but still valid approach would be
 * to leave the extensions features and then boot strap up the forth kernel. This approach
 * is to highlight certain features and bring them to the forefront of the implementation
 * lets start with the high level description of the text interpreter. The code includes
 * error handling which is left out for brevity. JonesForth leaves out this key too,
 * which is too bad because the code should have had it, to make it usable.
 * 
 * 1. get a word, a string of bytes 0 terminated and its length (look at code for specifics)
 * this is the fundamental unit of action within forth.
 * 2. Check if it is a number
 * 	a. if in interpreter (I_MODE) mode push the number on the stack [DONE]
 * 	b. if in compiler mode (C_MODE) compile code that pushes the number on the stack [DONE]
 * 3. Check if it is a local variable
 * 	a. C_MODE: compile code to push the local variable [DONE]
 * 4. Check if it is within the global dictionary, take action based on type
 * 	a. I_MODE: interpret the effects of the word if possible [DONE]
 * 	b. C_MODE: compile the word if possible. [DONE]
 * 5. Check the ending character of the word
 * 	a. '{' signifies we are defining a function word [DONE]
 * 	b. ':' signifies we are defining a constant [DONE]
 * 	c. ';' signifies we are defining a global variable. [DONE]
 * 	d. '.' signifies we are defining a local variable or assigning to it [DONE]
 * 6. Otherwise this word does not exist, print an error. [DONE]
 * 
 * Where [DONE] is found go to step 1.
 * 
 * Key observations include there is no tokenizer or parser. Words are simply looked up
 * and the respective action is taken. I check for numbers for first, this is not an error.
 * This decision allows for "60minutes" to be valid input so you can decorate your numbers
 * as you wish. Words may not start with numbers (0-9). Locals are supported and this is
 * key for using words as an abstraction to its fullest. Words are given types in the
 * dictionary and this allows for dispatch to deal with in either mode correctly.
 * Character endings dispatch to special handling of the word. This takes of the place of
 * special words such as ':' being traditionally used for function declaration. I find it
 * simpler and more consistent for a post fix approach.
 * 
 * In compile mode the stack is used for blocks. Traditional forths do the same however
 * I output enumerations to label the type of blocks. This allows for a singular '}'
 * to end all types of blocks (like C).
 * 
 * '(' ')' processes a list of words as locals similiar to forth stack notations, but is
 * usable anywhere in the function. Thus add{ ( arg1 arg2 ) arg1 arg2 + } would map
 * the stack values into the locals in the order they were pushed in locals seamlessly.
 * 
 * In a compiled word, '[' enters immediate mode, ']' exits it. This allows you to
 * do some calculations and put it into the current word as a constant. Also,
 * the calculation could be using globals which change based on configuration which
 * would result in different constants being planted into the code. Combine this conditional
 * folding of "if"s and you get conditional compilation. Sometimes you just want to
 * output a constant but show the work that went into it to make it more clear. Since the
 * work is being done in interpeter mode during compilation this saves time at run time.
 * 
 * 'if{' control flow word works as would expect, only in compile mode, with its optional
 * friend '}{' for else to be ended with '}'. They can be nested. I would like to add
 * basic constant checking to allow for the collapsing of 0 if{ stuff here } to nothing
 * and removal of the branching logic in 1 if{ stuff here }. This is effective reduction
 * of the code generation.
 * 
 * A singular loop, while condition if{ stuff here } , is provided that pairs up with
 * the above control flow to make the loop construct.
 * 
 * Tail Call Optimization: Yes. When you call any compiled function followed by 'ret'
 * it will be converted into a jump without building the stack. This allows you to
 * build loops out of recursive functions or simply functions as an optimization
 * when calling functions in the tail position as defined.
 * 
 * The dictionary is a linked list which is 2 byte aligned and uses 2 byte offsets
 * to link things together. See WordEntry struct for more information. This is the key
 * in the space effeicency of this impelementation alone with the lack of tokenizer and parser.
 * For example the entry for '+' is 12 bytes and supports being called directly by
 * the interpreter and copied inline when compiled. Amazing.
 * 
 * Code size is key, with this implementation producing smaller and faster native machine
 * code versus a STC, ITC, or DTC forth (see Moving Forth). This was a design goal.
 * 
 * getWord or Word is an essential function which called another even more important
 * function called "key". I have spent many hours pondering "key" and the functionality
 * being described around it, which had left me strait up puzzeled for a long time.
 * Some samples: Our cooperative OS changes tasking in "key". Call "key" to implement
 * custom parsing from within your words. | " some code" interpret | should do what you want.
 * 
 * Clear? In C you can call getChar(), but that gets input from the user. In Forth
 * "key myWord" would return 'm' so it is processing the next input directly. Key
 * is how the internal forth plumbing is working so you have access to the same input stream
 * as the interpreter. This is not the situation you normally find yourself in. This
 * unifying concept of all input streams are inputs directly in is facinating and 
 * a bit confusing coming from the C perspective. For it to also accept inputs from compiled
 * code feeding it back into the wood chipper is tricky as well.
 * 
 * Despite the mysteries and legends around it I think I have narrowed it down
 * into a que/stack of strings data structure with code to check for new characters being
 * available, entering into sleep with nothing to do, and checking the task queue to see
 * if other work has been registered to do from anything. I think this should be
 * better explained in the forth material as this is a kind of super power, especially in the
 * embedded space.
 * 
 * With this type of command line foundation you can unify the normal ideas held
 * in the command line. Bash invokes programs, feeds in inputs, can pipe things together.
 * This implementation would be able to compose programs, author new fast code,
 * take dynamic action, write and execute functionality without taking up space.
 * You have interpreter mode and when you need the other functionality you can
 * create a function and then call it and then "forget" it to erase it from the 
 * dictionary, allowing endless functions without filling up your dictionary or
 * for just temporarily redefining other words and then to roll them back. This
 * gives you power of C and Bash together, once you add in the "key" function or "word"
 * you can create custom DSL's to do very targeted specific things.
 * 
 * 
 * */

// TODO LIST
// make sure optimizations are correctly canceled in control flow ends
// Case statement (takes care of basic if else if else pattern) 
//
// DONE: Optimize conditionals
// DONE: Collapse if else when there is a constant input condition.
// DONE: Globals and Constants are compiled as functions
// DONE: optimize + - << >> with constants
// DONE: first pass inline ability
// DONE: '(' processing
// DONE: locals
// DONE: FORGET
// DONE: Immediate Mode and LIT special word
// DONE: print words
// DONE: strings
// DONE: characters

static Context dcx;

#define F_IMMED  0x80
#define F_HIDDEN 0x40
#define TOS 0
#define SCRATCH 1
#define CTX 4
#define RSP 4
#define G1 6
#define G2 7

/*e*/static void
mc_callRelative(u32 target, u32 currentAddr)/*i;*/
{
	u16 *outputCursor = (u16*)currentAddr;
	u32 currentPC = currentAddr + 4;
	u32 jump = (target - currentPC) >> 1;
	u32 imm11 = jump << 21 >> 21;
	u32 imm10 = jump << 11 >> 22;
	u32 i2    = jump << 10 >> 31;
	u32 i1    = jump << 9 >> 31;
	u32 S     = jump << 8 >> 31;
	u32 j2    = (i2^1)^S;
	u32 j1    = (i1^1)^S;
	u32 code = 0xF000;
	code += (S<<10) + imm10;
	*outputCursor++ = code;
	code = 0xD000;
	code += (j2<<11) + (j1<<13) + imm11;
	*outputCursor = code;
}

/*e*/static void
callWord(u32 target)/*i;*/
{
	dcx.hasCall = 1;
	u32 currentAddr = (u32)dcx.compileCursor;
	mc_callRelative(target, currentAddr);
	dcx.compileCursor += 2;
}

/*e*/static void
inlineWord(u16 *code)/*i;*/
{
	do {
		*dcx.compileCursor++ = *code++;
	} while (*code != 0x4770);
}

/*e*/static u32
armBranch(s32 imm)/*i;*/
{
	u32 code = 0xE000;
	if (imm < -1024 || imm > 1023) { return -1; }
	u32 encode = imm;
	code += (encode<<21)>>21;
	return code;
}

/*e*/static u32
armCond(u32 cond, s32 imm)/*i;*/
{
	u32 code = 0xD000;
	if (imm < -128 || imm > 127) { return -1; }
	u32 encode = imm;
	code += ((encode<<24)>>24) + (cond << 8);
	return code;
}

/*e*/static u32
armPush(u32 regBits)/*i;*/
{
	u32 code = 0xB400;
	code += regBits;
	return code;
}

/*e*/static u32
armPop(u32 regBits)/*i;*/
{
	u32 code = 0xBC00;
	code += regBits;
	return code;
}

/*e*/static u32
armBx(u32 reg)/*i;*/
{
	u32 code = 0x4700;
	code += reg << 3;
	return code;
}

/*e*/static u32
armMovImm(u32 dest, u32 val)/*i;*/
{
	u32 code = 0x2000;
	code += val + (dest << 8);
	return code;
}

/*e*/static u32
armMov(u32 dest, u32 src)/*i;*/
{
	u32 code = 0x4600;
	code += ((dest>>3)<<7) + ((dest<<29)>>29) + (src << 3);
	return code;
}

/*e*/static u32
armSubImm(u32 dest, u32 val)/*i;*/
{
	u32 code = 0x3800;
	code += val + (dest << 8);
	return code;
}

/*e*/static u32
armSub3(u32 dest, u32 arg1, u32 arg2)/*i;*/
{
	u32 code = 0x1A00;
	code += dest + (arg1 << 3) + (arg2 << 6);
	return code;
}

/*e*/static u32
armAddImm(u32 dest, u32 val)/*i;*/
{
	u32 code = 0x3000;
	code += val + (dest << 8);
	return code;
}

/*e*/static u32
armAdd3(u32 dest, u32 arg1, u32 arg2)/*i;*/
{
	u32 code = 0x1800;
	code += dest + (arg1 << 3) + (arg2 << 6);
	return code;
}

static u32
armLdrOffset(u32 dest, u32 src, u32 offset)
{
	u32 code = 0x6800;
	code += dest + (src << 3) + (offset << 6);
	return code;
}

static u32
armStrOffset(u32 src, u32 dest, u32 offset)
{
	u32 code = 0x6000;
	code += src + (dest << 3) + (offset << 6);
	return code;
}

static u32
armLslsImm(u32 dest, u32 src, u32 val)
{
	u32 code = 0x0000;
	code += dest + (src << 3) + ((val&0X1F) << 6);
	return code;
}

static u32
armLsls(u32 dest, u32 arg1)
{
	u32 code = 0x4080;
	code += dest + (arg1 << 3);
	return code;
}

static u32
armLsrsImm(u32 dest, u32 src, u32 val)
{
	u32 code = 0x0800;
	code += dest + (src << 3) + ((val&0X1F) << 6);
	return code;
}

static u32
armLsrs(u32 dest, u32 arg1)
{
	u32 code = 0x40C0;
	code += dest + (arg1 << 3);
	return code;
}

static u32
armCmpImm(u32 left, u32 val)
{
	u32 code = 0x2800;
	code += val + (left << 8);
	return code;
}

static u32
armCmp(u32 left, u32 right)
{
	u32 code = 0x4280;
	code += left + (right << 3);
	return code;
}

static void
mc_localLoadOptFixUp(u16 *code, u16 *dest)
{
	dest[0] = dest[1] + 1; // change destination to WRK from TOS
	dest[1] = (code[1] &~ 0x3F) + 0x08; // op TOS, WRK
}

static void
mc_localLoadOpt(u16 *code)
{
	if (dcx.compileCursor-2 == dcx.lastLocalLoad) {
		// we just pushed a local, re-write
		mc_localLoadOptFixUp(code, dcx.compileCursor - 2);
		return;
	}
	inlineWord(code);
}

static void
mc_stackAdd(u16 *code)
{
	if ( dcx.compileCursor-1 == dcx.lastSmallConst) {
		// we just pushed a small constant, re-write
		dcx.compileCursor -= 2;
		u32 val = (*dcx.lastSmallConst<<24)>>24;
		*dcx.compileCursor++ = armAddImm(TOS, val);
		return;
	}
	if (dcx.compileCursor-2 == dcx.lastLocalLoad) {
		// we just pushed a local, re-write
		mc_localLoadOptFixUp(code, dcx.compileCursor - 2);
		return;
	}
	inlineWord(code);
}

static void
mc_stackSub(u16 *code)
{
	if (dcx.compileCursor-1 == dcx.lastSmallConst) {
		// we just pushed a small constant, re-write
		dcx.compileCursor -= 2;
		u32 val = (*dcx.lastSmallConst<<24)>>24;
		*dcx.compileCursor++ = armSubImm(TOS, val);
		return;
	}
	if (dcx.compileCursor-2 == dcx.lastLocalLoad) {
		// we just pushed a local, re-write
		u16 *dest = dcx.compileCursor - 2;
		dest[0] = dest[1] + 1; // change destination to WRK from TOS
		dest[1] = code[1] + 0x38;
		return;
	}
	inlineWord(code);
}

static void
mc_stackLS(u16 *code)
{
	if (dcx.compileCursor-1 == dcx.lastSmallConst) {
		// we just pushed a small constant, re-write
		dcx.compileCursor -= 2;
		u32 val = (*dcx.lastSmallConst<<24)>>24;
		*dcx.compileCursor++ = armLslsImm(TOS, TOS, val);
		return;
	}
	if (dcx.compileCursor-2 == dcx.lastLocalLoad) {
		// we just pushed a local, re-write
		mc_localLoadOptFixUp(code, dcx.compileCursor - 2);
		return;
	}
	inlineWord(code);
}

static void
mc_stackRS(u16 *code)
{
	if (dcx.compileCursor-1 == dcx.lastSmallConst) {
		// we just pushed a small constant, re-write
		dcx.compileCursor -= 2;
		u32 val = (*dcx.lastSmallConst<<24)>>24;
		*dcx.compileCursor++ = armLsrsImm(TOS, TOS, val);
		return;
	}
	if (dcx.compileCursor-2 == dcx.lastLocalLoad) {
		// we just pushed a local, re-write
		mc_localLoadOptFixUp(code, dcx.compileCursor - 2);
		return;
	}
	inlineWord(code);
}

/*e*/static void
mc_condition(SmallContext *s)/*i;*/
{
	// compare top two items on the stack and consume them
	u32 prevCode = *(s->compileCursor-1);
	if ( (prevCode>>8) == 32)
	{
		// we just pushed a small constant, re-write
		s->compileCursor -= 2;
		u32 val = (prevCode<<24)>>24;
		*s->compileCursor++ = armCmpImm(TOS, val);
		*s->compileCursor++ = armPop(1<<TOS);
		return;
	}
	*s->compileCursor++ = armPop(1<<SCRATCH);
	*s->compileCursor++ = armCmp(SCRATCH,TOS);
	*s->compileCursor++ = armPop(1<<TOS);
}

/*e*/static void
mc_call(SmallContext *s)/*i;*/
{
	s->notLeaf = 1;
	*s->compileCursor++ = armMov(SCRATCH, TOS);
	*s->compileCursor++ = armPop(1<<TOS);
	*s->compileCursor++ = armMov(15, SCRATCH);
}

static u32
armAdr(u32 dest, u32 imm)
{
	u32 code = 0xA000;
	code += imm + (dest << 8);
	return code;
}


#define COND_BEQ 0x0
#define COND_BNE 0x1
#define COND_BGE 0xA
#define COND_BLT 0xB
#define COND_BGT 0xC
#define COND_BLE 0xD

enum{
	WORD_FUNC_BUILTIN,
	WORD_FUNC_COMP,
	WORD_FUNC_INLINE,
	WORD_FUNC_INLINE_LO,
	WORD_END_BLOCK,
	WORD_RET,
	WORD_GLOBAL,
	WORD_CONSTANT_SMALL,
	WORD_CONSTANT,
	WORD_IF,
	WORD_ELSE,
	WORD_WHILE,
	WORD_PLUS,
	WORD_MINUS,
	WORD_LS,
	WORD_RS,
	WORD_LPAREN,
	WORD_FORGET,
	WORD_LSQBRACKET,
	WORD_RSQBRACKET,
	WORD_MAKE_LIT,
	WORD_DOUBLEQUOTES,
	WORD_SINGLEQUOTES,
	WORD_EQUALS,
	WORD_NOT_EQUALS,
	WORD_LESS_THAN,
	WORD_GREATER_THAN,
	WORD_LESS_THAN_EQUAL,
	WORD_GREATER_THAN_EQUAL,
	WORD_LOCAL,
};

enum{
	BLOCK_WORD,
	BLOCK_COND,
	BLOCK_ELSE,
	BLOCK_WHILE,
	BLOCK_WHILE_COND,
};

enum{
	COM_START,
	COM_IN_COMMENT,
	COM_TOKEN,
};

static void
mc_loadLocal(u32 index)
{
	dcx.lastLocalLoad = dcx.compileCursor;
	*dcx.compileCursor++ = armPush(1<<TOS);
	*dcx.compileCursor++ = armLdrOffset(TOS, RSP, index);
}

static void
mc_storeLocal(u32 index)
{
	*dcx.compileCursor++ = armStrOffset(TOS, RSP, index);
	*dcx.compileCursor++ = armPop(1<<TOS);
}

static void
mc_addrOfLocal(SmallContext *s, u32 index)
{
	*s->compileCursor++ = armPush(1<<TOS);
	*s->compileCursor++ = armMov(TOS, RSP);
	if (index > 0)
	{
		*s->compileCursor++ = armAddImm(TOS, index < 2);
	}
}

static void
mc_loadGlobal(SmallContext *s, Word *word)
{
	*s->compileCursor++ = armPush(1<<TOS);
	u32 cfa = wordToCfa(word);
	u16 *globalIndex = (u16*)cfa;
	u32 index = *globalIndex;
	if (index < 32)
	{
		*s->compileCursor++ = armLdrOffset(TOS, G1, index);
	} else {
		*s->compileCursor++ = armLdrOffset(TOS, G2, index - 32);
	}
}

static void
mc_storeGlobal(SmallContext *s, Word *word)
{
	u32 cfa = wordToCfa(word);
	u16 *globalIndex = (u16*)cfa;
	u32 index = *globalIndex;
	if (index < 32)
	{
		*s->compileCursor++ = armStrOffset(TOS, G1, index);
	} else {
		*s->compileCursor++ = armStrOffset(TOS, G2, index - 32);
	}
	*s->compileCursor++ = armPop(1<<TOS);
}

static void
mc_addrOfGlobal(SmallContext *s, Word *word)
{
	*s->compileCursor++ = armPush(1<<TOS);
	u32 cfa = wordToCfa(word);
	u16 *globalIndex = (u16*)cfa;
	u32 index = *globalIndex;
	if (index < 32)
	{
		*s->compileCursor++ = armMov(TOS, G1);
	} else {
		*s->compileCursor++ = armMov(TOS, G2);
		index -= 32;
	}
	if (index > 0)
	{
		*s->compileCursor++ = armAddImm(TOS, index < 2);
	}
}


static u16*
d4th_getCode(WordEntry *w){u32 len=(w->keyLen + 2)/ 2; return &w->key[len]; }
static void d4th_pushValImm(s32 val) { dcx.psp--; *dcx.psp = val; }
static s32 d4th_popValImm(void) { s32 val = *dcx.psp; dcx.psp++; return val; }
static void d4th_compValImm(s32 value)
{
	*dcx.compileCursor++ = armPush(1<<TOS);
	if (value >= 0 && value <= 255) {
		dcx.lastSmallConst = dcx.compileCursor;
		*dcx.compileCursor++ = armMovImm(TOS, value);
	} else if (value >= -255 && value <= -1) {
		*dcx.compileCursor++ = armMovImm(TOS, -value);
		*dcx.compileCursor++ = 0x4240;
	} else {
		if ((u32)dcx.compileCursor & 0x02) { *dcx.compileCursor++ = armMov(8, 8); }
		*(u32*)dcx.compileCursor = 0xE0014800;
		dcx.compileCursor+= 2;
		*(u32*)dcx.compileCursor = value;
		dcx.compileCursor+= 2;
	}
}

static WordEntry*
d4th_createEntry(u8 *text, u32 len)
{
	WordEntry *current = dcx.dictionary;
	WordEntry *new = (WordEntry *)dcx.compileCursor;
	text[len] = 0;
	dcx.dictionary = new;
	new->next = (u32)new - (u32)current;
	new->keyLen = len;
	u32 i = 0;
	u32 wordLen = (len + 2)/ 2;
	u16 *newText	= new->key;
	u16 *wordText	= (u16*)text;
	do {
		newText[i] = wordText[i];
		i++;
	} while (i < wordLen);
	dcx.compileBase = new->key + wordLen;
	dcx.compileCursor = dcx.compileBase;
	return new;
}

static WordEntry*
d4th_lookUpInDictionary(u8 *text, u32 len)
{
	WordEntry 	*current	= dcx.dictionary;
	u32		wordLen		= (len + 1)/ 2;
	u16 		*wordText	= (u16*)text;
	goto start;
while (1) {
	next:{
	u32 increment = current->next;
	if (increment == 0) { return 0; }
	current = (WordEntry*)((u32)current - increment);}
	start:
	if (current->keyLen != len) { continue; }
	u32 i 		= 0;
	u16 *currText	= current->key;
	//~ io_printhn((u32)current->key);
	do {
		if (currText[i] != wordText[i]) { goto next; }
		i++;
	} while (i < wordLen);
	return current;
}
}

// creates a constant entry. Can be redefined, but its address cannot be taken
static void
d4th_createConstWord(u8 *text, u32 len)
{
	if (dcx.compileMode != 0) { io_printsn("[ERROR] Cannot create constant in compile mode."); return; }
	s32 val = d4th_popValImm();
	s32 type = val >= -255 && val <= 255 ? WORD_CONSTANT_SMALL : WORD_CONSTANT;
	text[len] = 0;
	WordEntry *word = d4th_lookUpInDictionary(text, len);
	if (word&&type==WORD_CONSTANT_SMALL&&word->type==WORD_CONSTANT_SMALL) {
		s16 *code = (s16*)d4th_getCode(word);
		*code = val;
		return;
	}
	WordEntry *new = d4th_createEntry(text, len);
	new->type = type;
	if (type == WORD_CONSTANT_SMALL) {
		*dcx.compileBase++ = val;
		dcx.compileCursor = dcx.compileBase;
	} else { // WORD_CONSTANT
		*dcx.compileBase++ = armPush(1<<TOS);
		if ((u32)dcx.compileBase & 0x02) { *dcx.compileBase++ = armMov(8, 8); }
		*(u32*)dcx.compileBase = 0x47704800;
		dcx.compileBase+= 2;
		*(u32*)dcx.compileBase = val;
		dcx.compileBase+= 2;
		dcx.compileCursor = dcx.compileBase;
	}
}

// get address of word 
static u16*
d4th_addrOfWord(u8 *text, u32 len)
{
	text[len] = 0;
	WordEntry *word = d4th_lookUpInDictionary(text, len);
	u16 *code = 0;
	if (word) {
	code = d4th_getCode(word);
	switch (word->type & 0x3F) {
	case WORD_FUNC_BUILTIN:  break;
	case WORD_FUNC_COMP:  break;
	case WORD_FUNC_INLINE:  break;
	case WORD_FUNC_INLINE_LO:  break;
	default: io_printsn("[ERROR] Word is wrong type to take address."); code = 0; break;
	}
	} else { io_printsn("[ERROR] Cannot find word to take address."); }
	return code;
}

// get address of word 
static void
d4th_addrOfWordPut(u8 *text, u32 len)
{
	u16 *code = d4th_addrOfWord(text, len);
	if (code == 0) { return; } 
	if (dcx.compileMode == 0) {
		d4th_pushValImm((s32)code + 1);
	} else {
		d4th_compValImm((s32)code + 1);
	}
}

static void
d4th_localWord(u8 *text, u32 len)
{
	// make sure we are in compile mode
	if (dcx.compileMode == 0) { io_printsn("[ERROR] Cannot create local in immediate mode."); return; }
	// look for existing local
	u32 localNum = tree_count(dcx.locals);
	Tree *local = tree_add(&dcx.locals, text, len, (void*)localNum);
	if (local) {
		localNum = (u32)local->value;
	}
	mc_storeLocal(localNum);
}

/*e*/static s32
consumeCharLit(void)/*i;*/
{
	s32 lit = f_key();
	if (lit != '\\') { return lit; }
	lit = f_key();
	switch (lit) {
		case 'n' : lit = 0x0A; break;
		case 'r' : lit = 0x0D; break;
		case 't' : lit = 0x09; break;
		case '"' : lit = '"' ; break;
		default  :  break;
	}
	return lit + 0x10000;
}

/*e*/static u8*
consumeStringLit(u8 *out)/*i;*/
{
	s32 lit;
	while ( (lit = consumeCharLit()) != '"' ) {
		*out++ = lit & 0xFF;
	}
	return out;
}

static void d4th_captureChar(void){d4th_pushValImm(consumeCharLit() & 0xFF);}
static void d4th_compileChar(void){d4th_compValImm(consumeCharLit() & 0xFF);}

static void
d4th_captureString(void)
{
	*dcx.compileCursor++ = armPush(1<<TOS);
	if ((u32)dcx.compileCursor & 0x02) { *dcx.compileCursor++ = armMov(8, 8); }
	// record address for getting address and jumping
	u16 *adrThenJump = dcx.compileCursor;
	// combine adr instruction and jump
	*adrThenJump++ = armAdr(0, 0);
	dcx.compileCursor += 2;
	u8 *start = (u8*)dcx.compileCursor;
	u8 *end = consumeStringLit(start);
	*end = 0;
	// calculate the length of the string in instructions
	u32 lengthInInstrs = (end-start + 2) / 2;
	// increment the compile cursor
	dcx.compileCursor += lengthInInstrs;
	// write out the adr and branch instruction
	*adrThenJump = armBranch(dcx.compileCursor-adrThenJump-2);
}

static void
d4th_lparen(void)
{
	u32 buff[32];
	u8 *start;
	u8 *end;
	u32 len;
	u16 *codeStart = dcx.compileCursor;
	
	// get words to make locals
	while (1) {
		start = (u8*)buff;
		end = d4th_getWord(start);
		len = end - start;
		if (len == 1 && *start==')') { break; }
		d4th_localWord(start, len);
	}
	// the code generated is in the opposite order we want, fix it
	u16 *codeEnd = dcx.compileCursor;
	u16 tmp;
	
	u16 *topCursor = codeStart;
	u16 *botCursor = codeEnd - 2;
	while (topCursor < botCursor) {
		tmp = *topCursor;
		*topCursor = *botCursor;
		*botCursor = tmp;
		topCursor += 2;
		botCursor -= 2;
	}
}

static void
d4th_createGlobalWord(u8 *text, u32 len)
{
	if (dcx.compileMode != 0) { io_printsn("[ERROR] Cannot create global in compile mode."); return; }
	WordEntry *new = d4th_createEntry(text, len);
	s32 val = d4th_popValImm();
	new->type = WORD_GLOBAL;
	*dcx.compileBase++ = armPush(1<<TOS);
	if ((u32)dcx.compileBase & 0x02) { *dcx.compileBase++ = armMov(8, 8); }
	*(u32*)dcx.compileBase = 0x4770A000;
	dcx.compileBase+= 2;
	*(u32*)dcx.compileBase = val;
	dcx.compileBase+= 2;
	dcx.compileCursor = dcx.compileBase;
}

static void
d4th_createFuncWord(u8 *text, u32 len)
{
	if (dcx.compileMode != 0) { io_printsn("[ERROR] Cannot create function in compile mode."); return; }
	dcx.compileMode = 1;
	d4th_pushValImm(BLOCK_WORD);
	WordEntry *new = d4th_createEntry(text, len);
	new->type = WORD_FUNC_COMP;
	*dcx.compileCursor++ = armMov(SCRATCH, 14);
	*dcx.compileCursor++ = armSubImm(RSP, (12*4));
	*dcx.compileCursor++ = armStrOffset(SCRATCH, RSP, 11);
}

static void
d4th_return(void)
{
	if (dcx.compileCursor-2 == dcx.lastCall) {
		// return after call, convert to jump
		io_printsn("[INFO] Tail Call.");
		dcx.compileCursor -= 2;
		u16 *newTarget = dcx.lastCallTarget + 3;
		dcx.lastCall		= 0;
		dcx.lastCallTarget	= 0;
		u32 branch = armBranch(newTarget - dcx.compileCursor - 2);
		if (branch != -1) {
			*dcx.compileCursor++ = branch;
		} else {
			callWord((u32)newTarget);
		}
		return;
	}
	// emit jump to end
	u32 branch = armBranch(dcx.returnMachineCode - dcx.compileCursor - 2);
	if (branch != -1) {
		*dcx.compileCursor = branch;
		/*if ( (branch+1) != *(dcx.compileCursor-1) )*/ {dcx.compileCursor++;}
	} else {
		dcx.returnMachineCode = dcx.compileCursor;
		*dcx.compileCursor++ = armLdrOffset(SCRATCH, RSP, 11);
		*dcx.compileCursor++ = armAddImm(RSP, (12*4));
		*dcx.compileCursor++ = armBx(SCRATCH);
	}
}

static void
d4th_makeLit(void)
{
	d4th_compValImm(d4th_popValImm());
}

static void
d4th_forget(void)
{
	WordEntry *current = dcx.dictionary;
	WordEntry *prev = (WordEntry*)((u32)current - (u32)current->next);
	dcx.compileBase = (u16*)current;
	dcx.compileCursor = dcx.compileBase;
	dcx.dictionary = prev;
	// if we output a return in machine code, clear the address
	if (dcx.returnMachineCode > dcx.compileBase) { dcx.returnMachineCode = 0; }
}

static void
d4th_endFunc(void)
{
	dcx.compileMode = 0;
	WordEntry *current = dcx.dictionary;
	u32 funcLen = dcx.compileCursor - dcx.compileBase;
	if (dcx.error != 0 || funcLen <= 3) {
		dcx.error=0;
		io_printsn("[ERROR] word canceled due to error.");
		d4th_forget();
		goto end;
	}
	if (dcx.hasCall==0 && (funcLen==4 || funcLen==5)) {
		io_printsn("[INFO] word will be inlined.");
		u32 copyAmt = funcLen - 3;
		u16 *to = dcx.compileBase;
		u16 *from = dcx.compileCursor - copyAmt;
		for (u32 i = 0; i < copyAmt; i++) {
			*to++ = *from++;
		}
		*to++ = 0x4770; // bx lr
		dcx.compileCursor = to;
		current->type = WORD_FUNC_INLINE;
		goto end;
	}
	//~ io_printin(dcx.compileCursor - dcx.compileBase);
	d4th_return();
	end: {
	funcLen = dcx.compileCursor - dcx.compileBase;
	tree_free(dcx.locals);
	dcx.locals = 0;
	dcx.hasCall = 0;
	// print out success message?
	io_printi(funcLen);
	io_prints(" instructions for word ");
	io_printsn((u8*)current->key);
	u16 *cursor = dcx.compileBase;
	while (cursor < dcx.compileCursor)
	{
		io_printhn(*cursor++);
	}
	}
	dcx.compileBase = dcx.compileCursor;
}

/*e*/static void
d4th_if(u16 *code)/*i;*/
{
	s32 block = d4th_popValImm();
	s32 delta = 0;
	if (block == BLOCK_WHILE) {
		delta = BLOCK_WHILE_COND - BLOCK_COND;
	} else {
		d4th_pushValImm(block);
	}
	if (dcx.compileCursor-2 == dcx.lastCompare) {
		// optimize comparison immediately before if
		dcx.compileCursor -= 2;
		if (dcx.compileCursor-1 == dcx.lastSmallConst) {
			// we just pushed a small constant, re-write
			dcx.compileCursor -= 2;
			u32 val = (*dcx.lastSmallConst<<24)>>24;
			*dcx.compileCursor++ = (*code++) + val;
			*dcx.compileCursor++ = *code;
			*dcx.compileCursor   = dcx.condCode;
		} else {
			code += 2;
			*dcx.compileCursor++ = *code++;
			*dcx.compileCursor++ = *code++;
			*dcx.compileCursor++ = *code;
			*dcx.compileCursor   = dcx.condCode;
		}
	} else if (dcx.compileCursor-1 == dcx.lastSmallConst) {
		// we just pushed a small constant, re-write
		dcx.compileCursor -= 2;
		u32 val = (*dcx.lastSmallConst<<24)>>24;
		*dcx.compileCursor   = val + 0x100;
	} else {
		*dcx.compileCursor++ = *code++;
		*dcx.compileCursor++ = *code;
		*dcx.compileCursor = 0;
	}
	d4th_pushValImm((s32)dcx.compileCursor);
	dcx.compileCursor++;
	d4th_pushValImm(BLOCK_COND+delta);
}

/*e*/static u32
d4th_closeIf(void)/*i;*/
{
	dcx.lastSmallConst = 0;
	dcx.lastCall = 0;
	dcx.lastCompare = 0;
	u16 *code = (u16*)d4th_popValImm();
	u32 cond = *code;
	if (cond>>8) {
		u32 smallConst = cond & 0xFF;
		if (smallConst) {
			*code = armMov(8, 8); // if is always true
			return 1;
		} else {
			dcx.compileCursor = code; // if is always false
			return 2;
		}
	}
	s32 res = armCond(cond,dcx.compileCursor - code - 2);
	if (res == -1) { io_printsn("[ERROR] branch is too large."); dcx.error=1; return 0; }
	*code = res;
	return 0;
}

/*e*/static void
d4th_closeElse(void)/*i;*/
{
	dcx.lastSmallConst = 0;
	dcx.lastCall = 0;
	dcx.lastCompare = 0;
	u16*code=(u16*)d4th_popValImm();
	u32 cond = *code;
	if (cond == 1) {
		dcx.compileCursor = code; // drop else block
	} else if (cond == 2) {
		*code = armMov(8, 8); // else always runs
	} else {
		*code=armBranch(dcx.compileCursor-code-2);
	}
}

/*e*/static void
d4th_closeWhile(void)/*i;*/
{
	s32 t=d4th_popValImm();
	u16*code=(u16*)d4th_popValImm();
	*dcx.compileCursor=armBranch(code-dcx.compileCursor-2);
	dcx.compileCursor++;
	d4th_pushValImm(t);
	u32 ifType = d4th_closeIf();
	if (ifType == 1) {
		// infinite loop, do nothing
	} else if (ifType == 2) {
		// loop never happens, nothing for us to do
	}
}

/*e*/static void
d4th_else(void)/*i;*/
{
	s32 block = d4th_popValImm();
	if (block != BLOCK_COND) { io_printsn("[ERROR] else must match if."); dcx.error=1; return; }
	u16 *elseLoc = dcx.compileCursor;
	*dcx.compileCursor++ = 0;
	u32 ifType = d4th_closeIf();
	if (ifType == 1) {
		*elseLoc = 1;
	} else if (ifType == 2) {
		elseLoc = dcx.compileCursor;
		*dcx.compileCursor++ = 2;
	}
	d4th_pushValImm((s32)elseLoc);
	d4th_pushValImm(BLOCK_ELSE);
}

/*e*/static void
d4th_while(void)/*i;*/
{
	d4th_pushValImm((s32)dcx.compileCursor);
	d4th_pushValImm(BLOCK_WHILE);
}

static void
d4th_endBlock(void)
{
	s32 block = d4th_popValImm();
	//~ io_printsn("endBLock");
	//~ io_printin(block);
	switch (block) {
	case BLOCK_WORD: d4th_endFunc(); break;
	case BLOCK_COND:{ d4th_closeIf(); break;}
	case BLOCK_ELSE:{ d4th_closeElse(); break;}
	case BLOCK_WHILE_COND:{ d4th_closeWhile(); break;}
	}
}

static void
d4th_interpretWord(WordEntry *word)
{
	u16 *code = d4th_getCode(word);
switch (word->type & 0x3F) {
	case WORD_FUNC_BUILTIN: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_FUNC_COMP: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_FUNC_INLINE: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_FUNC_INLINE_LO: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_END_BLOCK: io_printsn("[ERROR] Cannot close a block in intepreter mode."); break;
	case WORD_RET: break;
	case WORD_GLOBAL: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_CONSTANT_SMALL: d4th_pushValImm(*(s16*)code); break;
	case WORD_CONSTANT: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_IF: io_printsn("[ERROR] Cannot 'if{' in intepreter mode."); break;
	case WORD_ELSE: io_printsn("[ERROR] Cannot '}{' in intepreter mode."); break;
	case WORD_WHILE: io_printsn("[ERROR] Cannot 'while' in intepreter mode."); break;
	case WORD_PLUS: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_MINUS: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_LS: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_RS: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_LPAREN: io_printsn("[ERROR] Cannot '(' in intepreter mode."); break;
	case WORD_FORGET: d4th_forget(); break;
	case WORD_LSQBRACKET: /* no op */ break;
	case WORD_RSQBRACKET: dcx.compileMode = 1; break;
	case WORD_MAKE_LIT: d4th_makeLit(); break;
	case WORD_DOUBLEQUOTES: io_printsn("[ERROR] Cannot '\"' in intepreter mode."); break;
	case WORD_SINGLEQUOTES: d4th_captureChar(); break;
	case WORD_EQUALS: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_NOT_EQUALS: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_LESS_THAN: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_GREATER_THAN: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_LESS_THAN_EQUAL: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
	case WORD_GREATER_THAN_EQUAL: dcx.psp = fromC(dcx.rsp, dcx.psp, code); break;
}
}

static void
d4th_compileWord(WordEntry *word)
{
	u16 *code = d4th_getCode(word);
switch (word->type & 0x3F) {
	case WORD_FUNC_BUILTIN: callWord((u32)code); break;
	case WORD_FUNC_COMP: dcx.lastCall = dcx.compileCursor;
	dcx.lastCallTarget = code; callWord((u32)code); break;
	case WORD_FUNC_INLINE: inlineWord(code); break;
	case WORD_FUNC_INLINE_LO: mc_localLoadOpt(code); break;
	case WORD_END_BLOCK: d4th_endBlock(); break;
	case WORD_RET: d4th_return(); break;
	case WORD_GLOBAL: callWord((u32)code); break;
	case WORD_CONSTANT_SMALL: d4th_compValImm(*(s16*)code); break;
	case WORD_CONSTANT: callWord((u32)code); break;
	case WORD_IF: d4th_if(code); break;
	case WORD_ELSE: d4th_else(); break;
	case WORD_WHILE: d4th_while(); break;
	case WORD_PLUS: mc_stackAdd(code); break;
	case WORD_MINUS: mc_stackSub(code); break;
	case WORD_LS: mc_stackLS(code); break;
	case WORD_RS: mc_stackRS(code); break;
	case WORD_LPAREN: d4th_lparen(); break;
	case WORD_FORGET: io_printsn("[ERROR] Cannot 'FORGET' in compiler mode."); break;
	case WORD_LSQBRACKET: dcx.compileMode = 0; break;
	case WORD_RSQBRACKET: /* no op */ break;
	case WORD_MAKE_LIT: io_printsn("[ERROR] Cannot 'LIT' in compiler mode."); dcx.error=1; break;
	case WORD_DOUBLEQUOTES: d4th_captureString(); break;
	case WORD_SINGLEQUOTES: d4th_compileChar(); break;
	case WORD_EQUALS: dcx.lastCompare = dcx.compileCursor;
	dcx.condCode = COND_BNE; callWord((u32)code); break;
	case WORD_NOT_EQUALS: dcx.lastCompare = dcx.compileCursor;
	dcx.condCode = COND_BEQ; callWord((u32)code); break;
	case WORD_LESS_THAN: dcx.lastCompare = dcx.compileCursor;
	dcx.condCode = COND_BGE; callWord((u32)code); break;
	case WORD_GREATER_THAN: dcx.lastCompare = dcx.compileCursor;
	dcx.condCode = COND_BLE; callWord((u32)code); break;
	case WORD_LESS_THAN_EQUAL: dcx.lastCompare = dcx.compileCursor;
	dcx.condCode = COND_BGT; callWord((u32)code); break;
	case WORD_GREATER_THAN_EQUAL: dcx.lastCompare = dcx.compileCursor;
	dcx.condCode = COND_BLT; callWord((u32)code); break;
}
}

/*e*/
void
d4th_interpretText(void)/*p;*/
{
	//~ startSysTimer();
while (1) {
	//~ io_printin(endSysTimer());
	u32 buff[32];
	u8 *start = (u8*)buff;
	u8 *end = d4th_getWord(start);
	//~ startSysTimer();
	u32 len = end - start;
	// see if it is a number
	s32 result = f_s2i(start);
	if (result != 0 || *start == '0') {
		if (dcx.compileMode == 0) {
			d4th_pushValImm(result);
		} else {
			d4th_compValImm(result);
		} continue;
	}
	// check for local
	Tree *local = tree_find(dcx.locals, start, len);
	if (local) {
		mc_loadLocal((u32)local->value); continue;
	}
	// see if it is a word
	WordEntry *word = d4th_lookUpInDictionary(start, len);
	if (word) {
		if (dcx.compileMode == 0) {
			d4th_interpretWord(word);
			if (dcx.psp > (s32*)(END_OF_RAM - 516) ) {
				io_printsn("[ERROR] stack underflow!");
				dcx.psp	= (s32*)(END_OF_RAM - 516);
			}
		} else {
			d4th_compileWord(word);
		} continue;
	}
	u32 endChar = start[len-1];
	switch (endChar) {
	case ':' : d4th_createConstWord(start, len-1); continue;
	case '{' : d4th_createFuncWord(start, len-1); continue;
	case ';' : d4th_createGlobalWord(start, len-1); continue;
	case '.' : d4th_localWord(start, len-1); continue;
	case '@' : d4th_addrOfWordPut(start, len-1); continue;
	default: io_prints(start); io_printsn(" Word not found"); continue;
	}
}
}

/*e*/
void
dayForthInitP2(WordEntry *last)/*p;*/
{
	dcx.psp			= (void*)(END_OF_RAM - 516);
	dcx.rsp			= (void*)(END_OF_RAM - 1024);
	dcx.dictionary		= last;
	dcx.compileBase		= (void*)__bss_end__;
	dcx.compileCursor	= dcx.compileBase;
	io_printhn((u32)last);
	io_printhn((u32)dcx.compileBase);
}


