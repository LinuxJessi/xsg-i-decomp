"""Original character pointees inside a provisional anonymous DATA bucket.

The map view names SOURCE owners, never invented retail symbols. Its payload
comes from actual ordinary C OBJECTs, and the caller still proves selected
carve inputs and the complete normal link. Zero gaps are not text objects.
"""
import ast
import hashlib
import re
import struct
from pathlib import Path


def require(ok, message):
    if not ok:
        raise ValueError('original character subobject: '+message)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_reader(original, evidence, dc):
    """One closed original idiom: copy 17 pointers to stack, index into %s."""
    require(evidence.get('kind')=='stack_copy_indexed_printf_text_table/1',
            'unsupported original reader shape')
    symbols=[s for s in original.symbols if s.name==evidence['function'] and s.type==2]
    require(len(symbols)==1,'reader is not a unique genuine original function')
    function=symbols[0]
    points=evidence['sites']
    require(all(function.value<=int(value,0)<function.value+function.size
                for value in points.values()),'reader site escapes original function')
    def word(key, offset=0):
        address=int(points[key],0)+offset
        require(function.value<=address<function.value+function.size,'reader trace escapes function')
        return struct.unpack('<I',dc.va_bytes(original,address,4))[0]
    def i(key,opcode,rs,rt,immediate,offset=0):
        value=word(key,offset)
        require((value>>26,value>>21&31,value>>16&31,value&0xffff)==
                (opcode,rs,rt,immediate&0xffff),'original '+key+' instruction differs')
    def r(key,rs,rt,rd,shift,funct):
        value=word(key)
        require((value>>26,value>>21&31,value>>16&31,value>>11&31,value>>6&31,value&63)==
                (0,rs,rt,rd,shift,funct),'original '+key+' instruction differs')
    table=int(evidence['table_address'],0)
    require(table%8==0,'original pointer template is not 8-byte aligned')
    i('table_lui',15,0,2,((table+0x8000)>>16)&0xffff)
    i('table_addiu',9,2,10,table)
    r('stack_destination',29,0,11,0,45)
    i('table_alignment_test',12,10,5,7)
    branch=word('aligned_branch')
    require((branch>>26,branch>>21&31,branch>>16&31)==(4,5,0),
            'aligned original reader branch differs')
    signed=branch&0xffff
    if signed&0x8000:signed-=0x10000
    require(int(points['aligned_branch'],0)+4+signed*4==int(points['copy_limit'],0),
            'original aligned branch does not select the proved copy')
    i('copy_limit',9,10,2,64)
    # This is the original load/store compiler loop, not an authored ASM route.
    expected=[(55,10,7,0),(55,10,8,8),(55,10,9,16),(55,10,14,24),
              (63,11,7,0),(63,11,8,8),(63,11,9,16),(63,11,14,24),(9,10,10,32)]
    for offset,(op,rs,rt,immediate) in enumerate(expected):
        i('copy_loop',op,rs,rt,immediate,offset*4)
    require(word('copy_loop',36)==word('copy_loop',40)==0,'original copy loop scheduling differs')
    bne=word('copy_loop',44)
    require((bne>>26,bne>>21&31,bne>>16&31)==(5,10,2),'original copy loop bound differs')
    displacement=bne&0xffff
    if displacement&0x8000:displacement-=0x10000
    require(int(points['copy_loop'],0)+48+displacement*4==int(points['copy_loop'],0),
            'original copy loop branch target differs')
    i('copy_loop',9,11,11,32,48)
    i('tail_load',35,10,16,0)
    i('tail_store',43,11,16,0)
    i('state_load',32,22,7,0x48)
    r('state_index',0,7,2,2,0)
    r('stack_index',29,2,3,0,33)
    i('pointee_load',35,3,8,0)
    fmt=int(evidence['format_address'],0)
    i('format_lui',15,0,6,((fmt+0x8000)>>16)&0xffff)
    i('format_addiu',9,6,6,fmt)
    callees=[s for s in original.symbols if s.name==evidence['formatter'] and s.type==2]
    require(len(callees)==1 and evidence['formatter']=='xglFontDebugPrintf',
            'original formatter is not the evidenced variadic text API')
    call=word('formatter_call');site=int(points['formatter_call'],0)
    require(call>>26==3 and ((call&0x3ffffff)<<2 | ((site+4)&0xf0000000))==callees[0].value,
            'original JAL does not reach the genuine formatter')
    # x/y occupy r4/r5 and the format occupies r6. Its second conversion is
    # the r8 pointer. Only this closed %2d:%s format prefix is supported.
    formatting=dc.va_bytes(original,fmt,64).split(b'\0',1)[0]
    require(b'%2d:%s' in formatting and formatting.index(b'%2d:%s')==formatting.index(b'%'),
            'original format does not establish the r8 character-pointer argument')
    pointers=struct.unpack('<17I',dc.va_bytes(original,table,68))
    require(pointers[-1]==0,'original 17th copied pointer is not the terminal NULL')
    return dict(function=function.name,function_address=function.value,function_size=function.size,
                pointer_table_address=table,copied_pointer_bytes=68,copied_pointers=17,
                nonterminal_pointees=list(pointers[:16]),formatter=callees[0].name,
                formatter_address=callees[0].value,format_address=fmt,
                zero_padding_credited=False)


def verified_item(root, entry, record, ctx, original, location, dc):
    """Prove SOURCE/RAW storage before admitting one interior map view."""
    request=location.get('original_text_subobject')
    if request is None:return None
    require(record.get('unit')=='main' and location.get('section') in ('.rodata','.sdata'),
            'only initialized MAIN read-only character storage is supported')
    require(request.get('original_named_identity_absent') is True and
            request.get('source_coordinate_name_only') is True,
            'SOURCE names must not be presented as original ELF symbols')
    source_rel=next(p for p in record['files'] if p.endswith('.c'))
    source=Path(root)/source_rel
    require(sha(source)==record['files'][source_rel]['sha256'],'current source pin differs')
    object_info=record.get('compiler_object') or record['object']
    obj_path=Path(object_info['path'])
    require(sha(obj_path)==object_info['sha256'],'ordinary raw compiler object pin differs')
    raw=dc.Elf(obj_path.read_bytes())
    asm_path=Path(record['assembly']['path'])
    require(sha(asm_path)==record['assembly']['sha256'],'current compiler assembly pin differs')
    assembly=asm_path.read_text()
    source_text=source.read_text()
    name=request['source_name'];lo=int(location['address'],0);size=int(request['text_extent'])
    require(location.get('names')==[name] and 3<=size<=4096,'SOURCE owner name or text extent differs')
    require(name=='D_'+format(lo,'08X'),'SOURCE coordinate name does not match original VA')
    pattern=(r'(?<![\w])(?P<storage>static\s+)?const\s+char\s+'+re.escape(name)+
             r'\s*\[\s*'+str(size)+r'\s*\]\s*=\s*(?P<literal>"(?:\\.|[^"\\])*")\s*;')
    definitions=list(re.finditer(pattern,source_text))
    require(len(definitions)==1,'no unique ordinary const-char definition with the exact extent')
    value=ast.literal_eval(definitions[0]['literal']).encode('latin1')+b'\0'
    require(len(value)==size and b'\0' not in value[:-1],'definition is not one exact terminated text value')
    try:decoded=value[:-1].decode('ascii')
    except UnicodeDecodeError:
        try:decoded=value[:-1].decode('euc_jp')
        except UnicodeDecodeError as exc:raise ValueError('original character subobject: text encoding is not supported') from exc
    require(all(c.isprintable() or c in '\n\r\t' for c in decoded) and any(c.isalnum() for c in decoded),
            'payload is not meaningful printable text')
    expected=dc.va_bytes(original,lo,size)
    require(expected==value,'ordinary SOURCE literal differs from exact original text')
    require(not [s for s in original.symbols if s.name and 0<s.shndx<len(original.sections)
                 and original.sections[s.shndx].name==location['section'] and
                 (lo<=s.value<lo+size or s.type==1 and s.value<lo<s.value+s.size)],
            'a genuine retail identity intersects the proposed unnamed text')
    source_binding=0 if definitions[0]['storage'] else 1
    owners=[s for s in raw.symbols if s.name==name]
    require(len(owners)==1,'raw compiler char owner is not unique')
    owner=owners[0]
    require((owner.bind,owner.type,owner.size)==(source_binding,1,size),
            'raw compiler identity does not match actual SOURCE char storage')
    require(0<owner.shndx<len(raw.sections) and raw.sections[owner.shndx].name==location['section'],
            'raw SOURCE object is not in the original read-only section')
    section=raw.sections[owner.shndx]
    require(raw.section_bytes(section)[owner.value:owner.value+size]==value,
            'ordinary raw C initializer differs from original text')
    require(not [r for r in dc.read_relocations(raw).get(section.index,[])
                 if owner.value<=r[0]<owner.value+size],'char payload contains a relocation')
    # Require the named typed emission and label outside authored #APP blocks.
    app=False;outside=[]
    for line in assembly.splitlines():
        if line.strip()=='#APP':app=True
        elif line.strip()=='#NO_APP':app=False
        elif not app:outside.append(line)
    emission='\n'.join(outside)
    require(re.search(r'^'+re.escape(name)+r':\s*$',emission,re.M) and
            re.search(r'\.type\s+'+re.escape(name)+r',\s*@object',emission) and
            re.search(r'\.size\s+'+re.escape(name)+r',\s*'+str(size)+r'\b',emission),
            'actual char object is not ordinary outside-APP typed cc1 emission')
    reader=verify_reader(original,request['reader'],dc)
    require(lo in reader['nonterminal_pointees'],'original character table does not point at this exact string start')
    original_ctx=dict(ctx)
    original_ctx.pop('verified_original_character_subobjects',None)
    parents=[i for i in dc.section_items(original_ctx,location['section'])
             if i['name']==request['parent_name'] and i['start']<lo<lo+size<=i['end']
             and not i.get('source_coordinate_owner')]
    require(len(parents)==1,'no unique original containing anonymous scaffold bucket')
    parent=parents[0]
    parent_owners=[s for s in raw.symbols if s.name==parent['name'] and s.type==1 and s.size>0
                   and s.shndx==section.index]
    require(len(parent_owners)==1,'existing C prefix owner is absent or ambiguous')
    prefix=parent_owners[0]
    require(parent['start']+prefix.size<=lo and
            raw.section_bytes(section)[prefix.value:prefix.value+prefix.size]==
            dc.va_bytes(original,parent['start'],prefix.size),
            'existing exact C prefix overlaps new text or differs from original')
    proof=dict(kind='source_owned_original_character_subobject/1',source_name=name,
        section=location['section'],range=[lo,lo+size],size=size,decoded_text=decoded,
        original_named_symbols=[],original_binding_asserted=False,original_type_asserted=False,
        source_binding=source_binding,compiler_type='OBJECT',compiler_offset=owner.value,
        source_sha256=sha(source),compiler_raw_sha256=sha(obj_path),assembly_sha256=sha(asm_path),
        source_parent_name=parent['name'],original_parent_range=[parent['start'],parent['end']],
        existing_C_prefix_extent=prefix.size,reader=reader,
        actual_C_payload_sha256=hashlib.sha256(value).hexdigest(),padding_credited=False)
    return dict(start=lo,end=lo+size,name=name,labels=[name],
        source_coordinate_owner=True,original_character_subobject_proof=proof,
        original_parent=parent,block=dict(lines=[]))
