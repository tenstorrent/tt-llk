# Configuration Unit

The Configuration Unit can read or write [backend configuration](BackendConfiguration.md). Its `WRCFG` and `RDCFG` instructions access the same GPRs as the [Scalar Unit (ThCon)](ScalarUnit.md). The instructions executed by the Configuration Unit are:

<table><thead><tr><th>Instruction</th><th>Latency</th><th>Throughput</th></tr></thead>
<tr><td><code><a href="SETC16.md">SETC16</a></code></td><td>1 cycle</td><td>Issue one per thread per cycle</td></tr>
<tr><td><code><a href="WRCFG.md">WRCFG</a></code></td><td>2 cycles</td><td>Issue one per cycle</td></tr>
<tr><td><code><a href="RMWCIB.md">RMWCIB</a></code></td><td>1 cycle</td><td rowspan="5">Issue at most one of these per cycle, provided neither <code>RDCFG</code> nor <code>WRCFG</code> issued in previous cycle by any thread</td></tr>
<tr><td><code><a href="RDCFG.md">RDCFG</a></code></td><td>≥&nbsp;2&nbsp;cycles</td></tr>
<tr><td>RISCV&nbsp;read&nbsp;request</td><td>1 cycle</td></tr>
<tr><td>RISCV&nbsp;write&nbsp;request</td><td>1 cycle</td></tr>
<tr><td>Mover&nbsp;write&nbsp;request</td><td>1 cycle</td></tr>
</table>

At most three instructions can be accepted per cycle (one from each thread), subject to the limitations in the "Throughput" column above. Note that excessive use of `WRCFG` from one thread can starve `RMWCIB` and `RDCFG` from other threads. RISCV read-requests and write-requests against `TENSIX_CFG_BASE` also end up at the Configuration Unit, and it processes these requests as if they were single-cycle instructions. Notably, excessive use of `WRCFG` can also starve processing of these requests, causing high latency for read-requests and delayed processing of write-requests. The latter can expose race conditions in RISCV code if said code fails to consider proper memory ordering. Also note that while processing of these requests only takes a single cycle, it can take multiple cycles for the requests to travel from RISCV to the Configuration Unit, and multiple cycles for read-responses to return back to RISCV.
