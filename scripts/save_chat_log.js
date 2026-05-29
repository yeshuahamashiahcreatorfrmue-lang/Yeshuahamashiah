#!/usr/bin/env node
/* 채팅 원문 영구 보존기.
 * 세션 트랜스크립트(~/.claude/projects/<proj>/*.jsonl)에서 사용자 메시지 원문을 뽑아
 * docs/요청기록.json(원천) + docs/요청기록_원문.md(가독)로 "병합" 저장한다.
 * 병합이라 컨테이너 리셋·/clear·트랜스크립트 정리에도 기존 기록이 사라지지 않는다.
 * SessionStart 훅 + 수동 실행 둘 다로 호출됨. 실패해도 게임/세션에 영향 없음. */
const fs=require('fs'), path=require('path');
const REPO=process.cwd();
const TARGET=REPO;                                   // 이 프로젝트 cwd
const OUT_JSON=path.join(REPO,'docs','요청기록.json');
const OUT_MD=path.join(REPO,'docs','요청기록_원문.md');

function candidateDirs(){
  const roots=[process.env.HOME&&process.env.HOME+'/.claude/projects','/root/.claude/projects','/home/claude/.claude/projects'].filter(Boolean);
  const dirs=[];
  for(const r of roots){ if(!safe(()=>fs.statSync(r).isDirectory())) continue;
    for(const d of fs.readdirSync(r)){ const p=path.join(r,d); if(safe(()=>fs.statSync(p).isDirectory())) dirs.push(p); } }
  return [...new Set(dirs)];
}
function safe(fn){ try{return fn();}catch(e){return false;} }

function extract(){
  const rows=[];
  for(const dir of candidateDirs()){
    for(const f of fs.readdirSync(dir)){ if(!f.endsWith('.jsonl')) continue;
      let lines; try{ lines=fs.readFileSync(path.join(dir,f),'utf8').split('\n'); }catch(e){ continue; }
      for(const ln of lines){ if(!ln.trim()) continue; let o; try{o=JSON.parse(ln);}catch(e){continue;}
        if(o.type!=='user'||!o.message) continue;
        if(o.cwd && o.cwd!==TARGET) continue;                 // 이 프로젝트 것만
        let c=o.message.content, text=null;
        if(typeof c==='string') text=c;
        else if(Array.isArray(c)){ if(c.some(p=>p&&p.type==='tool_result')) continue; text=c.filter(p=>p&&p.type==='text').map(p=>p.text).join('\n'); }
        if(!text) continue; text=text.trim(); if(!text) continue;
        if(text.startsWith('<')||text.includes('system-reminder')||text.startsWith('[Request interrupted')||text.includes('Stop hook feedback')||text.startsWith('Caveat:')) continue;
        rows.push({t:o.timestamp||'',text});
      }
    }
  }
  return rows;
}

function load(){ try{ return JSON.parse(fs.readFileSync(OUT_JSON,'utf8')); }catch(e){ return []; } }

function main(){
  const merged=new Map();                              // key: t|text → 중복 제거(병합 보존)
  for(const r of load()) merged.set(r.t+'|'+r.text, r);
  for(const r of extract()) merged.set(r.t+'|'+r.text, r);
  const all=[...merged.values()].sort((a,b)=> a.t<b.t?-1 : a.t>b.t?1 : 0);
  fs.mkdirSync(path.dirname(OUT_JSON),{recursive:true});
  fs.writeFileSync(OUT_JSON, JSON.stringify(all,null,1));
  let md='# 요청기록 — 사용자 채팅 원문 전체 (자동 보존)\n\n'+
         '처음부터 현재까지, 시간순. 원문 그대로. (scripts/save_chat_log.js가 세션 시작마다 병합 갱신)\n\n';
  all.forEach((r,i)=>{ md+=`---\n**[${i+1}] ${r.t}**\n\n${r.text}\n\n`; });
  fs.writeFileSync(OUT_MD, md);
  console.log('[save_chat_log] '+all.length+'개 메시지 보존 → docs/요청기록_원문.md');
}
try{ main(); }catch(e){ console.error('[save_chat_log] skip:', e.message); }
