#include <ctype.h>

#include "ir.h"
#include "shl/shl-log.h"

static Str mnemonics[] = {
  STR_LIT("aaa"),
  STR_LIT("aad"),
  STR_LIT("aam"),
  STR_LIT("aas"),
  STR_LIT("adc"),
  STR_LIT("adcx"),
  STR_LIT("add"),
  STR_LIT("addpd"),
  STR_LIT("addps"),
  STR_LIT("addsd"),
  STR_LIT("addss"),
  STR_LIT("addsubpd"),
  STR_LIT("addsubps"),
  STR_LIT("adox"),
  STR_LIT("aesdec"),
  STR_LIT("aesdeclast"),
  STR_LIT("aesenc"),
  STR_LIT("aesenclast"),
  STR_LIT("aesimc"),
  STR_LIT("aeskeygenassist"),
  STR_LIT("and"),
  STR_LIT("andn"),
  STR_LIT("andpd"),
  STR_LIT("andps"),
  STR_LIT("andnpd"),
  STR_LIT("andnps"),
  STR_LIT("arpl"),
  STR_LIT("bextr"),
  STR_LIT("blendpd"),
  STR_LIT("blendps"),
  STR_LIT("blendvpd"),
  STR_LIT("blendvps"),
  STR_LIT("blsi"),
  STR_LIT("blsmsk"),
  STR_LIT("blsr"),
  STR_LIT("bndcl"),
  STR_LIT("bndcu"),
  STR_LIT("bndcn"),
  STR_LIT("bndldx"),
  STR_LIT("bndmk"),
  STR_LIT("bndmov"),
  STR_LIT("bndstx"),
  STR_LIT("bound"),
  STR_LIT("bsf"),
  STR_LIT("bsr"),
  STR_LIT("bswap"),
  STR_LIT("bt"),
  STR_LIT("btc"),
  STR_LIT("btr"),
  STR_LIT("bts"),
  STR_LIT("bzhi"),
  STR_LIT("call"),
  STR_LIT("cbw"),
  STR_LIT("cdq"),
  STR_LIT("cdqe"),
  STR_LIT("clac"),
  STR_LIT("clc"),
  STR_LIT("cld"),
  STR_LIT("cldemote"),
  STR_LIT("clflush"),
  STR_LIT("clflushopt"),
  STR_LIT("cli"),
  STR_LIT("clts"),
  STR_LIT("clwb"),
  STR_LIT("cmc"),
  STR_LIT("cmova"),
  STR_LIT("cmovae"),
  STR_LIT("cmovb"),
  STR_LIT("cmovbe"),
  STR_LIT("cmovc"),
  STR_LIT("cmove"),
  STR_LIT("cmovg"),
  STR_LIT("cmovge"),
  STR_LIT("cmovl"),
  STR_LIT("cmovle"),
  STR_LIT("cmovna"),
  STR_LIT("cmovnae"),
  STR_LIT("cmovnb"),
  STR_LIT("cmovnbe"),
  STR_LIT("cmovnc"),
  STR_LIT("cmovne"),
  STR_LIT("cmovng"),
  STR_LIT("cmovnge"),
  STR_LIT("cmovnl"),
  STR_LIT("cmovnle"),
  STR_LIT("cmovno"),
  STR_LIT("cmovnp"),
  STR_LIT("cmovns"),
  STR_LIT("cmovnz"),
  STR_LIT("cmovo"),
  STR_LIT("cmovp"),
  STR_LIT("cmovpe"),
  STR_LIT("cmovpo"),
  STR_LIT("cmovs"),
  STR_LIT("cmovz"),
  STR_LIT("cmp"),
  STR_LIT("cmppd"),
  STR_LIT("cmpps"),
  STR_LIT("cmps"),
  STR_LIT("cmpsb"),
  STR_LIT("cmpsw"),
  STR_LIT("cmpsd"),
  STR_LIT("cmpsq"),
  STR_LIT("cmpss"),
  STR_LIT("cmpxchg"),
  STR_LIT("cmpxchg8b"),
  STR_LIT("cmpxchg16b"),
  STR_LIT("comisd"),
  STR_LIT("comiss"),
  STR_LIT("cpuid"),
  STR_LIT("cqo"),
  STR_LIT("crc32"),
  STR_LIT("cvtdq2pd"),
  STR_LIT("cvtdq2ps"),
  STR_LIT("cvtpd2dq"),
  STR_LIT("cvtpd2pi"),
  STR_LIT("cvtpd2ps"),
  STR_LIT("cvtpi2pd"),
  STR_LIT("cvtpi2ps"),
  STR_LIT("cvtps2dq"),
  STR_LIT("cvtps2pd"),
  STR_LIT("cvtps2pi"),
  STR_LIT("cvtsd2si"),
  STR_LIT("cvtsd2ss"),
  STR_LIT("cvtsi2sd"),
  STR_LIT("cvtsi2ss"),
  STR_LIT("cvtss2sd"),
  STR_LIT("cvtss2si"),
  STR_LIT("cvttpd2dq"),
  STR_LIT("cvttpd2pi"),
  STR_LIT("cvttps2dq"),
  STR_LIT("cvttps2pi"),
  STR_LIT("cvttsd2si"),
  STR_LIT("cvttss2si"),
  STR_LIT("cwd"),
  STR_LIT("cwde"),
  STR_LIT("daa"),
  STR_LIT("das"),
  STR_LIT("dec"),
  STR_LIT("div"),
  STR_LIT("divpd"),
  STR_LIT("divps"),
  STR_LIT("divsd"),
  STR_LIT("divss"),
  STR_LIT("dppd"),
  STR_LIT("dpps"),
  STR_LIT("emms"),
  STR_LIT("enter"),
  STR_LIT("extractps"),
  STR_LIT("gf2p8affineinvqb"),
  STR_LIT("gf2p8affineqb"),
  STR_LIT("gf2p8mulb"),
  STR_LIT("haddpd"),
  STR_LIT("haddps"),
  STR_LIT("hlt"),
  STR_LIT("hsubpd"),
  STR_LIT("hsubps"),
  STR_LIT("idiv"),
  STR_LIT("imul"),
  STR_LIT("in"),
  STR_LIT("inc"),
  STR_LIT("ins"),
  STR_LIT("insb"),
  STR_LIT("insw"),
  STR_LIT("insd"),
  STR_LIT("insertps"),
  STR_LIT("int"),
  STR_LIT("into"),
  STR_LIT("int1"),
  STR_LIT("int3"),
  STR_LIT("invd"),
  STR_LIT("invlpg"),
  STR_LIT("invpcid"),
  STR_LIT("iret"),
  STR_LIT("iretd"),
  STR_LIT("iretq"),
  STR_LIT("ja"),
  STR_LIT("jae"),
  STR_LIT("jb"),
  STR_LIT("jbe"),
  STR_LIT("jc"),
  STR_LIT("jcxz"),
  STR_LIT("jecxz"),
  STR_LIT("jrcxz"),
  STR_LIT("je"),
  STR_LIT("jg"),
  STR_LIT("jge"),
  STR_LIT("jl"),
  STR_LIT("jle"),
  STR_LIT("jna"),
  STR_LIT("jnae"),
  STR_LIT("jnb"),
  STR_LIT("jnbe"),
  STR_LIT("jnc"),
  STR_LIT("jne"),
  STR_LIT("jng"),
  STR_LIT("jnge"),
  STR_LIT("jnl"),
  STR_LIT("jnle"),
  STR_LIT("jno"),
  STR_LIT("jnp"),
  STR_LIT("jns"),
  STR_LIT("jnz"),
  STR_LIT("jo"),
  STR_LIT("jp"),
  STR_LIT("jpe"),
  STR_LIT("jpo"),
  STR_LIT("js"),
  STR_LIT("jz"),
  STR_LIT("jmp"),
  STR_LIT("kaddw"),
  STR_LIT("kaddb"),
  STR_LIT("kaddq"),
  STR_LIT("kaddd"),
  STR_LIT("kandw"),
  STR_LIT("kandb"),
  STR_LIT("kandq"),
  STR_LIT("kandd"),
  STR_LIT("kandnw"),
  STR_LIT("kandnb"),
  STR_LIT("kandnq"),
  STR_LIT("kandnd"),
  STR_LIT("kmovw"),
  STR_LIT("kmovb"),
  STR_LIT("kmovq"),
  STR_LIT("kmovd"),
  STR_LIT("knotw"),
  STR_LIT("knotb"),
  STR_LIT("knotq"),
  STR_LIT("knotd"),
  STR_LIT("korw"),
  STR_LIT("korb"),
  STR_LIT("korq"),
  STR_LIT("kord"),
  STR_LIT("kortestw"),
  STR_LIT("kortestb"),
  STR_LIT("kortestq"),
  STR_LIT("kortestd"),
  STR_LIT("kshiftlw"),
  STR_LIT("kshiftlb"),
  STR_LIT("kshiftlq"),
  STR_LIT("kshiftld"),
  STR_LIT("kshiftrw"),
  STR_LIT("kshiftrb"),
  STR_LIT("kshiftrq"),
  STR_LIT("kshiftrd"),
  STR_LIT("ktestw"),
  STR_LIT("ktestb"),
  STR_LIT("ktestq"),
  STR_LIT("ktestd"),
  STR_LIT("kunpckbw"),
  STR_LIT("kunpckwd"),
  STR_LIT("kunpckdq"),
  STR_LIT("kxnorw"),
  STR_LIT("kxnorb"),
  STR_LIT("kxnorq"),
  STR_LIT("kxnord"),
  STR_LIT("kxorw"),
  STR_LIT("kxorb"),
  STR_LIT("kxorq"),
  STR_LIT("kxord"),
  STR_LIT("lahf"),
  STR_LIT("lar"),
  STR_LIT("lddqu"),
  STR_LIT("ldmxcsr"),
  STR_LIT("lea"),
  STR_LIT("leave"),
  STR_LIT("lfence"),
  STR_LIT("lgdt"),
  STR_LIT("lidt"),
  STR_LIT("lldt"),
  STR_LIT("lmsw"),
  STR_LIT("lock"),
  STR_LIT("lods"),
  STR_LIT("lodsb"),
  STR_LIT("lodsw"),
  STR_LIT("lodsd"),
  STR_LIT("lodsq"),
  STR_LIT("loop"),
  STR_LIT("loopz"),
  STR_LIT("loopnz"),
  STR_LIT("loope"),
  STR_LIT("loopne"),
  STR_LIT("lds"),
  STR_LIT("les"),
  STR_LIT("lfs"),
  STR_LIT("lgs"),
  STR_LIT("lss"),
  STR_LIT("lsl"),
  STR_LIT("ltr"),
  STR_LIT("lzcnt"),
  STR_LIT("maskmovdqu"),
  STR_LIT("maskmovq"),
  STR_LIT("maxpd"),
  STR_LIT("maxps"),
  STR_LIT("maxsd"),
  STR_LIT("maxss"),
  STR_LIT("mfence"),
  STR_LIT("minpd"),
  STR_LIT("minps"),
  STR_LIT("minsd"),
  STR_LIT("minss"),
  STR_LIT("monitor"),
  STR_LIT("mov"),
  STR_LIT("movapd"),
  STR_LIT("movaps"),
  STR_LIT("movbe"),
  STR_LIT("movd"),
  STR_LIT("movq"),
  STR_LIT("movddup"),
  STR_LIT("movdiri"),
  STR_LIT("movdir64b"),
  STR_LIT("movdqa"),
  STR_LIT("movdqu"),
  STR_LIT("movdq2q"),
  STR_LIT("movhlps"),
  STR_LIT("movhpd"),
  STR_LIT("movhps"),
  STR_LIT("movlhps"),
  STR_LIT("movlpd"),
  STR_LIT("movlps"),
  STR_LIT("movmskpd"),
  STR_LIT("movmskps"),
  STR_LIT("movntdqa"),
  STR_LIT("movntdq"),
  STR_LIT("movnti"),
  STR_LIT("movntpd"),
  STR_LIT("movntps"),
  STR_LIT("movntq"),
  STR_LIT("movq2dq"),
  STR_LIT("movs"),
  STR_LIT("movsb"),
  STR_LIT("movsw"),
  STR_LIT("movsd"),
  STR_LIT("movsq"),
  STR_LIT("movshdup"),
  STR_LIT("movsldup"),
  STR_LIT("movss"),
  STR_LIT("movsx"),
  STR_LIT("movsxd"),
  STR_LIT("movupd"),
  STR_LIT("movups"),
  STR_LIT("movzx"),
  STR_LIT("mpsadbw"),
  STR_LIT("mul"),
  STR_LIT("mulpd"),
  STR_LIT("mulps"),
  STR_LIT("mulsd"),
  STR_LIT("mulss"),
  STR_LIT("mulx"),
  STR_LIT("mwait"),
  STR_LIT("neg"),
  STR_LIT("nop"),
  STR_LIT("not"),
  STR_LIT("or"),
  STR_LIT("orpd"),
  STR_LIT("orps"),
  STR_LIT("out"),
  STR_LIT("outs"),
  STR_LIT("outsb"),
  STR_LIT("outsw"),
  STR_LIT("outsd"),
  STR_LIT("pabsb"),
  STR_LIT("pabsw"),
  STR_LIT("pabsd"),
  STR_LIT("pabsq"),
  STR_LIT("packsswb"),
  STR_LIT("packssdw"),
  STR_LIT("packusdw"),
  STR_LIT("packuswb"),
  STR_LIT("paddb"),
  STR_LIT("paddw"),
  STR_LIT("paddd"),
  STR_LIT("paddq"),
  STR_LIT("paddsb"),
  STR_LIT("paddsw"),
  STR_LIT("paddusb"),
  STR_LIT("paddusw"),
  STR_LIT("palignr"),
  STR_LIT("pand"),
  STR_LIT("pandn"),
  STR_LIT("pause"),
  STR_LIT("pavgb"),
  STR_LIT("pavgw"),
  STR_LIT("pblendvb"),
  STR_LIT("pblendw"),
  STR_LIT("pclmulqdq"),
  STR_LIT("pcmpeqb"),
  STR_LIT("pcmpeqw"),
  STR_LIT("pcmpeqd"),
  STR_LIT("pcmpeqq"),
  STR_LIT("pcmpestri"),
  STR_LIT("pcmpestrm"),
  STR_LIT("pcmpgtb"),
  STR_LIT("pcmpgtw"),
  STR_LIT("pcmpgtd"),
  STR_LIT("pcmpgtq"),
  STR_LIT("pcmpistri"),
  STR_LIT("pcmpistrm"),
  STR_LIT("pdep"),
  STR_LIT("pext"),
  STR_LIT("pextrb"),
  STR_LIT("pextrd"),
  STR_LIT("pextrq"),
  STR_LIT("pextrw"),
  STR_LIT("phaddw"),
  STR_LIT("phaddd"),
  STR_LIT("phaddsw"),
  STR_LIT("phminposuw"),
  STR_LIT("phsubw"),
  STR_LIT("phsubd"),
  STR_LIT("phsubsw"),
  STR_LIT("pinsrb"),
  STR_LIT("pinsrd"),
  STR_LIT("pinsrq"),
  STR_LIT("pinsrw"),
  STR_LIT("pmaddubsw"),
  STR_LIT("pmaddwd"),
  STR_LIT("pmaxsb"),
  STR_LIT("pmaxsw"),
  STR_LIT("pmaxsd"),
  STR_LIT("pmaxsq"),
  STR_LIT("pmaxub"),
  STR_LIT("pmaxuw"),
  STR_LIT("pmaxud"),
  STR_LIT("pmaxuq"),
  STR_LIT("pminsb"),
  STR_LIT("pminsw"),
  STR_LIT("pminsd"),
  STR_LIT("pminsq"),
  STR_LIT("pminub"),
  STR_LIT("pminuw"),
  STR_LIT("pminud"),
  STR_LIT("pminuq"),
  STR_LIT("pmovmskb"),
  STR_LIT("pmovsx"),
  STR_LIT("pmovzx"),
  STR_LIT("pmuldq"),
  STR_LIT("pmulhrsw"),
  STR_LIT("pmulhuw"),
  STR_LIT("pmulhw"),
  STR_LIT("pmulld"),
  STR_LIT("pmullq"),
  STR_LIT("pmullw"),
  STR_LIT("pmuludq"),
  STR_LIT("pop"),
  STR_LIT("popa"),
  STR_LIT("popad"),
  STR_LIT("popcnt"),
  STR_LIT("popf"),
  STR_LIT("popfd"),
  STR_LIT("popfq"),
  STR_LIT("por"),
  STR_LIT("prefetchh"),
  STR_LIT("prefetchw"),
  STR_LIT("psadbw"),
  STR_LIT("pshufb"),
  STR_LIT("pshufd"),
  STR_LIT("pshufhw"),
  STR_LIT("pshuflw"),
  STR_LIT("pshufw"),
  STR_LIT("psignb"),
  STR_LIT("psignw"),
  STR_LIT("psignd"),
  STR_LIT("pslldq"),
  STR_LIT("psllw"),
  STR_LIT("pslld"),
  STR_LIT("psllq"),
  STR_LIT("psraw"),
  STR_LIT("psrad"),
  STR_LIT("psraq"),
  STR_LIT("psrldq"),
  STR_LIT("psrlw"),
  STR_LIT("psrld"),
  STR_LIT("psrlq"),
  STR_LIT("psubb"),
  STR_LIT("psubw"),
  STR_LIT("psubd"),
  STR_LIT("psubq"),
  STR_LIT("psubsb"),
  STR_LIT("psubsw"),
  STR_LIT("psubusb"),
  STR_LIT("psubusw"),
  STR_LIT("punpckhbw"),
  STR_LIT("punpckhwd"),
  STR_LIT("punpckhdq"),
  STR_LIT("punpckhqdq"),
  STR_LIT("punpcklbw"),
  STR_LIT("punpcklwd"),
  STR_LIT("punpckldq"),
  STR_LIT("punpcklqdq"),
  STR_LIT("push"),
  STR_LIT("pusha"),
  STR_LIT("pushad"),
  STR_LIT("pushf"),
  STR_LIT("pushfd"),
  STR_LIT("pushfq"),
  STR_LIT("pxor"),
  STR_LIT("rcl"),
  STR_LIT("rcr"),
  STR_LIT("rep"),
  STR_LIT("repe"),
  STR_LIT("repne"),
  STR_LIT("repz"),
  STR_LIT("repnz"),
  STR_LIT("ret"),
  STR_LIT("retf"),
  STR_LIT("rol"),
  STR_LIT("ror"),
  STR_LIT("rcpps"),
  STR_LIT("rcpss"),
  STR_LIT("rdfsbase"),
  STR_LIT("rdgsbase"),
  STR_LIT("rdmsr"),
  STR_LIT("rdpid"),
  STR_LIT("rdpkru"),
  STR_LIT("rdpmc"),
  STR_LIT("rdrand"),
  STR_LIT("rdseed"),
  STR_LIT("rdtsc"),
  STR_LIT("rdtscp"),
  STR_LIT("rorx"),
  STR_LIT("roundpd"),
  STR_LIT("roundps"),
  STR_LIT("roundsd"),
  STR_LIT("roundss"),
  STR_LIT("rsm"),
  STR_LIT("rsqrtps"),
  STR_LIT("rsqrtss"),
  STR_LIT("sahf"),
  STR_LIT("sal"),
  STR_LIT("sar"),
  STR_LIT("sarx"),
  STR_LIT("sbb"),
  STR_LIT("scas"),
  STR_LIT("scasb"),
  STR_LIT("scasw"),
  STR_LIT("scasd"),
  STR_LIT("seta"),
  STR_LIT("setae"),
  STR_LIT("setb"),
  STR_LIT("setbe"),
  STR_LIT("setc"),
  STR_LIT("sete"),
  STR_LIT("setg"),
  STR_LIT("setge"),
  STR_LIT("setl"),
  STR_LIT("setle"),
  STR_LIT("setna"),
  STR_LIT("setnae"),
  STR_LIT("setnb"),
  STR_LIT("setnbe"),
  STR_LIT("setnc"),
  STR_LIT("setne"),
  STR_LIT("setng"),
  STR_LIT("setnge"),
  STR_LIT("setnl"),
  STR_LIT("setnle"),
  STR_LIT("setno"),
  STR_LIT("setnp"),
  STR_LIT("setns"),
  STR_LIT("setnz"),
  STR_LIT("seto"),
  STR_LIT("setp"),
  STR_LIT("setpe"),
  STR_LIT("setpo"),
  STR_LIT("sets"),
  STR_LIT("setz"),
  STR_LIT("sfence"),
  STR_LIT("sgdt"),
  STR_LIT("sha1rnds4"),
  STR_LIT("sha1nexte"),
  STR_LIT("sha1msg1"),
  STR_LIT("sha1msg2"),
  STR_LIT("sha256rnds2"),
  STR_LIT("sha256msg1"),
  STR_LIT("sha256msg2"),
  STR_LIT("shl"),
  STR_LIT("shld"),
  STR_LIT("shlx"),
  STR_LIT("shr"),
  STR_LIT("shrd"),
  STR_LIT("shrx"),
  STR_LIT("shufpd"),
  STR_LIT("shufps"),
  STR_LIT("sidt"),
  STR_LIT("sldt"),
  STR_LIT("smsw"),
  STR_LIT("sqrtpd"),
  STR_LIT("sqrtps"),
  STR_LIT("sqrtsd"),
  STR_LIT("sqrtss"),
  STR_LIT("stac"),
  STR_LIT("stc"),
  STR_LIT("std"),
  STR_LIT("sti"),
  STR_LIT("stmxcsr"),
  STR_LIT("stos"),
  STR_LIT("stosb"),
  STR_LIT("stosw"),
  STR_LIT("stosd"),
  STR_LIT("stosq"),
  STR_LIT("str"),
  STR_LIT("sub"),
  STR_LIT("subpd"),
  STR_LIT("subps"),
  STR_LIT("subsd"),
  STR_LIT("subss"),
  STR_LIT("swapgs"),
  STR_LIT("syscall"),
  STR_LIT("sysenter"),
  STR_LIT("sysexit"),
  STR_LIT("sysret"),
  STR_LIT("test"),
  STR_LIT("tpause"),
  STR_LIT("tzcnt"),
  STR_LIT("ucomisd"),
  STR_LIT("ucomiss"),
  STR_LIT("ud"),
  STR_LIT("umonitor"),
  STR_LIT("umwait"),
  STR_LIT("unpckhpd"),
  STR_LIT("unpckhps"),
  STR_LIT("unpcklpd"),
  STR_LIT("unpcklps"),
  STR_LIT("valignd"),
  STR_LIT("valignq"),
  STR_LIT("vblendmpd"),
  STR_LIT("vblendmps"),
  STR_LIT("vbroadcast"),
  STR_LIT("vcompresspd"),
  STR_LIT("vcompressps"),
  STR_LIT("vcvtpd2qq"),
  STR_LIT("vcvtpd2udq"),
  STR_LIT("vcvtpd2uqq"),
  STR_LIT("vcvtph2ps"),
  STR_LIT("vcvtps2ph"),
  STR_LIT("vcvtps2udq"),
  STR_LIT("vcvtps2qq"),
  STR_LIT("vcvtps2uqq"),
  STR_LIT("vcvtqq2pd"),
  STR_LIT("vcvtqq2ps"),
  STR_LIT("vcvtsd2usi"),
  STR_LIT("vcvtss2usi"),
  STR_LIT("vcvttpd2qq"),
  STR_LIT("vcvttpd2udq"),
  STR_LIT("vcvttpd2uqq"),
  STR_LIT("vcvttps2udq"),
  STR_LIT("vcvttps2qq"),
  STR_LIT("vcvttps2uqq"),
  STR_LIT("vcvttsd2usi"),
  STR_LIT("vcvttss2usi"),
  STR_LIT("vcvtudq2pd"),
  STR_LIT("vcvtudq2ps"),
  STR_LIT("vcvtuqq2pd"),
  STR_LIT("vcvtuqq2ps"),
  STR_LIT("vcvtusi2sd"),
  STR_LIT("vcvtusi2ss"),
  STR_LIT("vdbpsadbw"),
  STR_LIT("vexpandpd"),
  STR_LIT("vexpandps"),
  STR_LIT("verr"),
  STR_LIT("verw"),
  STR_LIT("vextractf128"),
  STR_LIT("vextractf32x4"),
  STR_LIT("vextractf64x2"),
  STR_LIT("vextractf32x8"),
  STR_LIT("vextractf64x4"),
  STR_LIT("vextracti128"),
  STR_LIT("vextracti32x4"),
  STR_LIT("vextracti64x2"),
  STR_LIT("vextracti32x8"),
  STR_LIT("vextracti64x4"),
  STR_LIT("vfixupimmpd"),
  STR_LIT("vfixupimmps"),
  STR_LIT("vfixupimmsd"),
  STR_LIT("vfixupimmss"),
  STR_LIT("vfmadd132pd"),
  STR_LIT("vfmadd213pd"),
  STR_LIT("vfmadd231pd"),
  STR_LIT("vfmadd132ps"),
  STR_LIT("vfmadd213ps"),
  STR_LIT("vfmadd231ps"),
  STR_LIT("vfmadd132sd"),
  STR_LIT("vfmadd213sd"),
  STR_LIT("vfmadd231sd"),
  STR_LIT("vfmadd132ss"),
  STR_LIT("vfmadd213ss"),
  STR_LIT("vfmadd231ss"),
  STR_LIT("vfmaddsub132pd"),
  STR_LIT("vfmaddsub213pd"),
  STR_LIT("vfmaddsub231pd"),
  STR_LIT("vfmaddsub132ps"),
  STR_LIT("vfmaddsub213ps"),
  STR_LIT("vfmaddsub231ps"),
  STR_LIT("vfmsubadd132pd"),
  STR_LIT("vfmsubadd213pd"),
  STR_LIT("vfmsubadd231pd"),
  STR_LIT("vfmsubadd132ps"),
  STR_LIT("vfmsubadd213ps"),
  STR_LIT("vfmsubadd231ps"),
  STR_LIT("vfmsub132pd"),
  STR_LIT("vfmsub213pd"),
  STR_LIT("vfmsub231pd"),
  STR_LIT("vfmsub132ps"),
  STR_LIT("vfmsub213ps"),
  STR_LIT("vfmsub231ps"),
  STR_LIT("vfmsub132sd"),
  STR_LIT("vfmsub213sd"),
  STR_LIT("vfmsub231sd"),
  STR_LIT("vfmsub132ss"),
  STR_LIT("vfmsub213ss"),
  STR_LIT("vfmsub231ss"),
  STR_LIT("vfnmadd132pd"),
  STR_LIT("vfnmadd213pd"),
  STR_LIT("vfnmadd231pd"),
  STR_LIT("vfnmadd132ps"),
  STR_LIT("vfnmadd213ps"),
  STR_LIT("vfnmadd231ps"),
  STR_LIT("vfnmadd132sd"),
  STR_LIT("vfnmadd213sd"),
  STR_LIT("vfnmadd231sd"),
  STR_LIT("vfnmadd132ss"),
  STR_LIT("vfnmadd213ss"),
  STR_LIT("vfnmadd231ss"),
  STR_LIT("vfnmsub132pd"),
  STR_LIT("vfnmsub213pd"),
  STR_LIT("vfnmsub231pd"),
  STR_LIT("vfnmsub132ps"),
  STR_LIT("vfnmsub213ps"),
  STR_LIT("vfnmsub231ps"),
  STR_LIT("vfnmsub132sd"),
  STR_LIT("vfnmsub213sd"),
  STR_LIT("vfnmsub231sd"),
  STR_LIT("vfnmsub132ss"),
  STR_LIT("vfnmsub213ss"),
  STR_LIT("vfnmsub231ss"),
  STR_LIT("vfpclasspd"),
  STR_LIT("vfpclassps"),
  STR_LIT("vfpclasssd"),
  STR_LIT("vfpclassss"),
  STR_LIT("vgatherdpd"),
  STR_LIT("vgatherqpd"),
  STR_LIT("vgatherdps"),
  STR_LIT("vgatherqps"),
  STR_LIT("vgetexppd"),
  STR_LIT("vgetexpps"),
  STR_LIT("vgetexpsd"),
  STR_LIT("vgetexpss"),
  STR_LIT("vgetmantpd"),
  STR_LIT("vgetmantps"),
  STR_LIT("vgetmantsd"),
  STR_LIT("vgetmantss"),
  STR_LIT("vinsertf128"),
  STR_LIT("vinsertf32x4"),
  STR_LIT("vinsertf64x2"),
  STR_LIT("vinsertf32x8"),
  STR_LIT("vinsertf64x4"),
  STR_LIT("vinserti128"),
  STR_LIT("vinserti32x4"),
  STR_LIT("vinserti64x2"),
  STR_LIT("vinserti32x8"),
  STR_LIT("vinserti64x4"),
  STR_LIT("vmaskmov"),
  STR_LIT("vmovdqa32"),
  STR_LIT("vmovdqa64"),
  STR_LIT("vmovdqu8"),
  STR_LIT("vmovdqu16"),
  STR_LIT("vmovdqu32"),
  STR_LIT("vmovdqu64"),
  STR_LIT("vpblendd"),
  STR_LIT("vpblendmb"),
  STR_LIT("vpblendmw"),
  STR_LIT("vpblendmd"),
  STR_LIT("vpblendmq"),
  STR_LIT("vpbroadcastb"),
  STR_LIT("vpbroadcastw"),
  STR_LIT("vpbroadcastd"),
  STR_LIT("vpbroadcastq"),
  STR_LIT("vpbroadcast"),
  STR_LIT("vpbroadcastm"),
  STR_LIT("vpcmpb"),
  STR_LIT("vpcmpub"),
  STR_LIT("vpcmpd"),
  STR_LIT("vpcmpud"),
  STR_LIT("vpcmpq"),
  STR_LIT("vpcmpuq"),
  STR_LIT("vpcmpw"),
  STR_LIT("vpcmpuw"),
  STR_LIT("vpcompressd"),
  STR_LIT("vpcompressq"),
  STR_LIT("vpconflictd"),
  STR_LIT("vpconflictq"),
  STR_LIT("vperm2f128"),
  STR_LIT("vperm2i128"),
  STR_LIT("vpermb"),
  STR_LIT("vpermd"),
  STR_LIT("vpermw"),
  STR_LIT("vpermi2b"),
  STR_LIT("vpermi2w"),
  STR_LIT("vpermi2d"),
  STR_LIT("vpermi2q"),
  STR_LIT("vpermi2ps"),
  STR_LIT("vpermi2pd"),
  STR_LIT("vpermilpd"),
  STR_LIT("vpermilps"),
  STR_LIT("vpermpd"),
  STR_LIT("vpermps"),
  STR_LIT("vpermq"),
  STR_LIT("vpermt2b"),
  STR_LIT("vpermt2w"),
  STR_LIT("vpermt2d"),
  STR_LIT("vpermt2q"),
  STR_LIT("vpermt2ps"),
  STR_LIT("vpermt2pd"),
  STR_LIT("vpexpandd"),
  STR_LIT("vpexpandq"),
  STR_LIT("vpgatherdd"),
  STR_LIT("vpgatherqd"),
  STR_LIT("vpgatherdq"),
  STR_LIT("vpgatherqq"),
  STR_LIT("vplzcntd"),
  STR_LIT("vplzcntq"),
  STR_LIT("vpmadd52huq"),
  STR_LIT("vpmadd52luq"),
  STR_LIT("vpmaskmov"),
  STR_LIT("vpmovb2m"),
  STR_LIT("vpmovw2m"),
  STR_LIT("vpmovd2m"),
  STR_LIT("vpmovq2m"),
  STR_LIT("vpmovdb"),
  STR_LIT("vpmovsdb"),
  STR_LIT("vpmovusdb"),
  STR_LIT("vpmovdw"),
  STR_LIT("vpmovsdw"),
  STR_LIT("vpmovusdw"),
  STR_LIT("vpmovm2b"),
  STR_LIT("vpmovm2w"),
  STR_LIT("vpmovm2d"),
  STR_LIT("vpmovm2q"),
  STR_LIT("vpmovqb"),
  STR_LIT("vpmovsqb"),
  STR_LIT("vpmovusqb"),
  STR_LIT("vpmovqd"),
  STR_LIT("vpmovsqd"),
  STR_LIT("vpmovusqd"),
  STR_LIT("vpmovqw"),
  STR_LIT("vpmovsqw"),
  STR_LIT("vpmovusqw"),
  STR_LIT("vpmovwb"),
  STR_LIT("vpmovswb"),
  STR_LIT("vpmovuswb"),
  STR_LIT("vprold"),
  STR_LIT("vprolvd"),
  STR_LIT("vprolq"),
  STR_LIT("vprolvq"),
  STR_LIT("vprord"),
  STR_LIT("vprorvd"),
  STR_LIT("vprorq"),
  STR_LIT("vprorvq"),
  STR_LIT("vpscatterdd"),
  STR_LIT("vpscatterdq"),
  STR_LIT("vpscatterqd"),
  STR_LIT("vpscatterqq"),
  STR_LIT("vpsllvw"),
  STR_LIT("vpsllvd"),
  STR_LIT("vpsllvq"),
  STR_LIT("vpsravw"),
  STR_LIT("vpsravd"),
  STR_LIT("vpsravq"),
  STR_LIT("vpsrlvw"),
  STR_LIT("vpsrlvd"),
  STR_LIT("vpsrlvq"),
  STR_LIT("vpternlogd"),
  STR_LIT("vpternlogq"),
  STR_LIT("vptestmb"),
  STR_LIT("vptestmw"),
  STR_LIT("vptestmd"),
  STR_LIT("vptestmq"),
  STR_LIT("vptestnmb"),
  STR_LIT("vptestnmw"),
  STR_LIT("vptestnmd"),
  STR_LIT("vptestnmq"),
  STR_LIT("vrangepd"),
  STR_LIT("vrangeps"),
  STR_LIT("vrangesd"),
  STR_LIT("vrangess"),
  STR_LIT("vrcp14pd"),
  STR_LIT("vrcp14sd"),
  STR_LIT("vrcp14ps"),
  STR_LIT("vrcp14ss"),
  STR_LIT("vreducepd"),
  STR_LIT("vreducesd"),
  STR_LIT("vreduceps"),
  STR_LIT("vreducess"),
  STR_LIT("vrndscalepd"),
  STR_LIT("vrndscalesd"),
  STR_LIT("vrndscaleps"),
  STR_LIT("vrndscaless"),
  STR_LIT("vrsqrt14pd"),
  STR_LIT("vrsqrt14sd"),
  STR_LIT("vrsqrt14ps"),
  STR_LIT("vrsqrt14ss"),
  STR_LIT("vscalefpd"),
  STR_LIT("vscalefsd"),
  STR_LIT("vscalefps"),
  STR_LIT("vscalefss"),
  STR_LIT("vscatterdps"),
  STR_LIT("vscatterdpd"),
  STR_LIT("vscatterqps"),
  STR_LIT("vscatterqpd"),
  STR_LIT("vshuff32x4"),
  STR_LIT("vshuff64x2"),
  STR_LIT("vshufi32x4"),
  STR_LIT("vshufi64x2"),
  STR_LIT("vtestpd"),
  STR_LIT("vtestps"),
  STR_LIT("vzeroall"),
  STR_LIT("vzeroupper"),
  STR_LIT("wait"),
  STR_LIT("wbinvd"),
  STR_LIT("wrfsbase"),
  STR_LIT("wrgsbase"),
  STR_LIT("wrmsr"),
  STR_LIT("wrpkru"),
  STR_LIT("xacquire"),
  STR_LIT("xrelease"),
  STR_LIT("xabort"),
  STR_LIT("xadd"),
  STR_LIT("xbegin"),
  STR_LIT("xchg"),
  STR_LIT("xend"),
  STR_LIT("xgetbv"),
  STR_LIT("xlat"),
  STR_LIT("xlatb"),
  STR_LIT("xor"),
  STR_LIT("xorpd"),
  STR_LIT("xorps"),
  STR_LIT("xrstor"),
  STR_LIT("xrstors"),
  STR_LIT("xsave"),
  STR_LIT("xsavec"),
  STR_LIT("xsaveopt"),
  STR_LIT("xsaves"),
  STR_LIT("xsetbv"),
  STR_LIT("xtest"),
  STR_LIT("fabs"),
  STR_LIT("fadd"),
  STR_LIT("faddp"),
  STR_LIT("fbld"),
  STR_LIT("fbstp"),
  STR_LIT("fchs"),
  STR_LIT("fclex"),
  STR_LIT("fnclex"),
  STR_LIT("fcmovcc"),
  STR_LIT("fcom"),
  STR_LIT("fcomp"),
  STR_LIT("fcompp"),
  STR_LIT("fcomi"),
  STR_LIT("fcomip"),
  STR_LIT("fucomi"),
  STR_LIT("fucomip"),
  STR_LIT("fcos"),
  STR_LIT("fdecstp"),
  STR_LIT("fdiv"),
  STR_LIT("fdivp"),
  STR_LIT("fdivr"),
  STR_LIT("ffree"),
  STR_LIT("fiadd"),
  STR_LIT("ficom"),
  STR_LIT("ficomp"),
  STR_LIT("fidiv"),
  STR_LIT("fidivr"),
  STR_LIT("fdivrp"),
  STR_LIT("fild"),
  STR_LIT("fimul"),
  STR_LIT("fincstp"),
  STR_LIT("finit"),
  STR_LIT("fninit"),
  STR_LIT("fist"),
  STR_LIT("fistp"),
  STR_LIT("fisttp"),
  STR_LIT("fisub"),
  STR_LIT("fisubr"),
  STR_LIT("fld"),
  STR_LIT("fld1"),
  STR_LIT("fldl2e"),
  STR_LIT("fldl2t"),
  STR_LIT("fldlg2"),
  STR_LIT("fldln2"),
  STR_LIT("fldpi"),
  STR_LIT("fldz"),
  STR_LIT("fldcw"),
  STR_LIT("fldenv"),
  STR_LIT("fmul"),
  STR_LIT("fmulp"),
  STR_LIT("fnop"),
  STR_LIT("fpatan"),
  STR_LIT("fprem"),
  STR_LIT("fprem1"),
  STR_LIT("fptan"),
  STR_LIT("frndint"),
  STR_LIT("frstor"),
  STR_LIT("fsave"),
  STR_LIT("fnsave"),
  STR_LIT("fscale"),
  STR_LIT("fsin"),
  STR_LIT("fsincos"),
  STR_LIT("fsqrt"),
  STR_LIT("fst"),
  STR_LIT("fstp"),
  STR_LIT("fstcw"),
  STR_LIT("fnstcw"),
  STR_LIT("fstenv"),
  STR_LIT("fnstenv"),
  STR_LIT("fstsw"),
  STR_LIT("fnstsw"),
  STR_LIT("fsub"),
  STR_LIT("fsubp"),
  STR_LIT("fsubr"),
  STR_LIT("fsubrp"),
  STR_LIT("ftst"),
  STR_LIT("fucom"),
  STR_LIT("fucomp"),
  STR_LIT("fwait"),
  STR_LIT("fucompp"),
  STR_LIT("fxam"),
  STR_LIT("fxch"),
  STR_LIT("fxrstor"),
  STR_LIT("fxsave"),
  STR_LIT("fxtract"),
  STR_LIT("fyl2x"),
  STR_LIT("fyl2xp1"),
  STR_LIT("f2xm1"),
};

static Str regs[] = {
  STR_LIT("rax"), STR_LIT("eax"),  STR_LIT("ax"),   STR_LIT("al"),
  STR_LIT("rcx"), STR_LIT("ecx"),  STR_LIT("cx"),   STR_LIT("cl"),
  STR_LIT("rdx"), STR_LIT("edx"),  STR_LIT("dx"),   STR_LIT("dl"),
  STR_LIT("rbx"), STR_LIT("ebx"),  STR_LIT("bx"),   STR_LIT("bl"),
  STR_LIT("rsp"), STR_LIT("esp"),  STR_LIT("sp"),   STR_LIT("spl"),
  STR_LIT("rbp"), STR_LIT("ebp"),  STR_LIT("bp"),   STR_LIT("bpl"),
  STR_LIT("rsi"), STR_LIT("esi"),  STR_LIT("si"),   STR_LIT("sil"),
  STR_LIT("rdi"), STR_LIT("edi"),  STR_LIT("di"),   STR_LIT("dil"),
  STR_LIT("r8"),  STR_LIT("r8d"),  STR_LIT("r8w"),  STR_LIT("r8b"),
  STR_LIT("r9"),  STR_LIT("r9d"),  STR_LIT("r9w"),  STR_LIT("r9b"),
  STR_LIT("r10"), STR_LIT("r10d"), STR_LIT("r10w"), STR_LIT("r10b"),
  STR_LIT("r11"), STR_LIT("r11d"), STR_LIT("r11w"), STR_LIT("r11b"),
  STR_LIT("r12"), STR_LIT("r12d"), STR_LIT("r12w"), STR_LIT("r12b"),
  STR_LIT("r13"), STR_LIT("r13d"), STR_LIT("r13w"), STR_LIT("r13b"),
  STR_LIT("r14"), STR_LIT("r14d"), STR_LIT("r14w"), STR_LIT("r14b"),
  STR_LIT("r15"), STR_LIT("r15d"), STR_LIT("r15w"), STR_LIT("r15b"),
};

static bool str_begins_with(Str str, Str prefix) {
  if (str.len < prefix.len)
    return false;

  str.len = prefix.len;
  return str_eq(str, prefix);
}

static void str_chop(Str *str, u32 n) {
  str->ptr += n;
  str->len -= n;
}

static Str str_chop_until(Str *str, char _char) {
  Str result = { str->ptr, 0 };
  while (result.len < str->len && result.ptr[result.len] != _char)
    ++result.len;
  if (result.len == str->len)
    return (Str) { NULL, (u32) -1 };
  str_chop(str, result.len + 1);
  return result;
}

static void str_trim_left(Str *str) {
  while (str->len > 0 && isspace(str->ptr[0])) {
    ++str->ptr;
    --str->len;
  }
}

static bool str_ieq(Str a, Str b) {
  if (a.len != b.len)
    return false;

  for (u32 i = 0; i < a.len; ++i)
    if (tolower(a.ptr[i]) != tolower(b.ptr[i]))
      return false;

  return true;
}

static bool str_is_number(Str str) {
  for (u32 i = 0; i < str.len; ++i)
    if (!isdigit(str.ptr[i]))
      return false;

  return true;
}

static bool mnemonic_to_instr_kind(Str mnemonic, EsmInstrKind *kind) {
  for (u32 i = 0; i < ARRAY_LEN(mnemonics); ++i) {
    if (str_ieq(mnemonic, mnemonics[i])) {
      *kind = i;
      return true;
    }
  }

  return false;
}

static bool str_to_instr_arg(Str str, EsmInstrArg *arg) {
  if (str_is_number(str)) {
    arg->kind = EsmInstrArgKindImm;
    arg->as.imm = str_to_i64(str);
    return true;
  }

  for (u32 i = 0; i < ARRAY_LEN(regs); ++i) {
    if (str_ieq(str, regs[i])) {
      arg->kind = EsmInstrArgKindReg;
      arg->as.reg.kind = i / 4;
      arg->as.reg.size = 1;
      for (u32 j = 0; j < 3 - i % 4; ++j)
        arg->as.reg.size *= 2;
      return true;
    }
  }

  if (str.ptr[0] != '[') {
    Str prefix = str_chop_until(&str, '[');
    if (prefix.len == (u32) -1) {
      arg->kind = EsmInstrArgKindLabel;
      arg->as.label = str;

      return true;
    }

    --str.ptr;
    ++str.len;

    if (str_ieq(prefix, STR_LIT("qword")))
      arg->as.mem.size = 8;
    else if (str_ieq(prefix, STR_LIT("dword")))
      arg->as.mem.size = 4;
    else if (str_ieq(prefix, STR_LIT("word")))
      arg->as.mem.size = 2;
    else if (str_ieq(prefix, STR_LIT("byte")))
      arg->as.mem.size = 1;
    else
      return false;
  }

  if (str.ptr[0] != '[')
    return false;

  ++str.ptr;
  --str.len;

  Str base = str_chop_until(&str, '+');
  if (base.len == (u32) -1) {
    base = str_chop_until(&str, ']');
    if (base.len == (u32) -1)
      return false;
  } else {
    ++str.ptr;
    --str.len;
  }

  arg->as.mem.base.kind = EsmInstrArgBaseKindLabel;
  arg->as.mem.base.as.label = base;

  for (u32 i = 0; i < ARRAY_LEN(regs); ++i) {
    if (str_ieq(base, regs[i])) {
      if (i % 4 != 0)
        return false;

      arg->as.mem.base.kind = EsmInstrArgBaseKindReg;
      arg->as.mem.base.as.reg = i / 4;
    }
  }

  arg->as.mem.has_index = false;
  arg->as.mem.scale = 0;
  arg->as.mem.displacement = 0;

  if (str.len > 0 && str.ptr[0] != ']') {
    bool is_index = true;
    Str value = str_chop_until(&str, '*');
    if (value.len == (u32) -1) {
      is_index = false;
      value = str_chop_until(&str, '+');
      if (value.len == (u32) -1) {
        value = str_chop_until(&str, ']');
        if (value.len == (u32) -1)
          return false;
      }
    }

    if (is_index) {
      bool found = false;

      for (u32 i = 0; i < ARRAY_LEN(regs); ++i) {
        if (str_ieq(value, regs[i])) {
          if (i % 4 != 0)
            return false;

          arg->as.mem.index = i / 4;
        }
      }

      if (!found)
        return false;

      bool is_end = false;
      Str scale = str_chop_until(&str, '+');
      if (scale.len == (u32) -1) {
        is_end = true;
        scale = str_chop_until(&str, ']');
      }
      if (scale.len != (u32) -1) {
        if (!str_is_number(scale))
          return false;
        arg->as.mem.scale = strtol(scale.ptr, NULL, 10);

        bool found = false;

        static u8 array[] = { 1, 2, 4, 8 };
        for (u32 i = 0; i < ARRAY_LEN(array); ++i) {
          if (arg->as.mem.scale == array[i]) {
            arg->as.mem.scale = i;
            found = true;
            break;
          }
        }

        if (!found)
          return false;
      }

      if (!is_end) {
        Str displacement = str_chop_until(&str, ']');
        if (displacement.len == (u32) -1)
          return false;

        if (!str_is_number(displacement))
          return false;

        arg->as.mem.displacement = strtol(displacement.ptr, NULL, 10);
      }
    } else {
      if (!str_is_number(value))
        return false;

      arg->as.mem.displacement = strtol(value.ptr, NULL, 10);
    }
  }

  arg->kind = EsmInstrArgKindMem;

  return true;
}

bool str_to_esm_ir(Str str, char *file_path, EsmIr *ir) {
  EsmSectionKind section_kind = EsmSectionKindText;
  u32 row = 1;

  while (str.len > 0) {
    if (str_begins_with(str, STR_LIT("section "))) {
      if (str_begins_with(str, STR_LIT("section '.text'\n"))) {
        section_kind = EsmSectionKindText;
        str_chop(&str, STR_LIT("section '.text'\n").len);
      } else if (str_eq(str, STR_LIT("section '.text'"))) {
        section_kind = EsmSectionKindText;
        str_chop(&str, STR_LIT("section '.text'").len);
      } else if (str_begins_with(str, STR_LIT("section '.data'\n"))) {
        section_kind = EsmSectionKindData;
        str_chop(&str, STR_LIT("section '.data'\n").len);
      } else if (str_eq(str, STR_LIT("section '.data'"))) {
        section_kind = EsmSectionKindData;
        str_chop(&str, STR_LIT("section '.data'").len);
      } else {
        ERROR("%s:%u: invalid `section` directive\n", file_path, row);
        return false;
      }

      ++row;
    } else if (str_begins_with(str, STR_LIT("global "))) {
      Str directive = str_chop_until(&str, '\n');
      if (directive.len == (u32) -1) {
        directive = str;
        str = (Str) {0};
      }

      str_chop(&directive, STR_LIT("global ").len);

      EsmExport export = { directive, section_kind, 0 };
      DA_APPEND(ir->exports, export);

      ++row;
    } else {
      Str instruction_str = str_chop_until(&str, '\n');
      if (instruction_str.len == (u32) -1) {
        instruction_str = str;
        str = (Str) {0};
      }
      str_trim_left(&instruction_str);

      Str label_str = str_chop_until(&instruction_str, ':');

      EsmSection *section = ir->sections + section_kind;

      if (label_str.len != (u32) -1) {
        str_trim_left(&instruction_str);

        EsmLabel label = { label_str, section->entries.len, 0 };
        DA_APPEND(section->labels, label);
      }

      if (instruction_str.len == 0) {
        ++row;
        continue;
      }

      Str mnemonic = str_chop_until(&instruction_str, ' ');
      if (mnemonic.len == (u32) -1) {
        mnemonic = instruction_str;
        instruction_str = (Str) {0};
      }
      str_trim_left(&instruction_str);

      EsmInstr instr = {0};
      if (!mnemonic_to_instr_kind(mnemonic, &instr.kind)) {
        ERROR("%s:%u: invalid mnemonic: "STR_FMT"\n",
              file_path, row, STR_ARG(mnemonic));
        return false;
      }

      for (u32 i = 0;
           i < ARRAY_LEN(instr.args) && instruction_str.len > 0;
           ++i) {
        Str arg_str = str_chop_until(&instruction_str, ',');
        if (arg_str.len == (u32) -1) {
          arg_str = instruction_str;
          instruction_str = (Str) {0};
        }
        str_trim_left(&instruction_str);
        if (!str_to_instr_arg(arg_str, instr.args + i)) {
          ERROR("%s:%u: invalid instruction argument: "STR_FMT"\n",
                file_path, row, STR_ARG(arg_str));
          return false;
        }
        ++instr.args_len;
      }

      EsmEntry entry = {
        EsmEntryKindInstr,
        { .instr = instr },
        row,
      };
      DA_APPEND(section->entries, entry);

      ++row;
    }
  }

  return true;
}

void esm_ir_free(EsmIr *ir) {
  for (u32 i = 0; i < EsmSectionKindsCount; ++i) {
    for (u32 j = 0; j < ir->sections[i].entries.len; ++j)
      if (ir->sections[i].entries.items[j].kind == EsmEntryKindData)
        free(ir->sections[i].entries.items[j].as.data.data);
    if (ir->sections[i].entries.items)
      free(ir->sections[i].entries.items);
    if (ir->sections[i].labels.items)
      free(ir->sections[i].labels.items);
  }
  if (ir->exports.items)
    free(ir->exports.items);
}
