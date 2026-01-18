# Mynta Masternode Guide

A complete guide to understanding, operating, and profiting from Mynta masternodes.

---

## Table of Contents

1. [What is a Masternode?](#what-is-a-masternode)
2. [Why Run a Masternode?](#why-run-a-masternode)
3. [How Do Masternodes Work?](#how-do-masternodes-work)
4. [Masternode Economics](#masternode-economics)
5. [Earning Potential Calculator](#earning-potential-calculator)
6. [Getting Started](#getting-started)
7. [Costs and Considerations](#costs-and-considerations)
8. [Frequently Asked Questions](#frequently-asked-questions)

---

## What is a Masternode?

A masternode is a server on the Mynta network that performs special functions beyond what regular nodes do. Think of it as a "premium" network participant that helps keep the blockchain running smoothly in exchange for regular rewards.

**In Simple Terms:**
- A regular node validates transactions and stores the blockchain
- A masternode does all that PLUS provides enhanced services to the network
- In return, masternode operators receive a share of every block reward

Masternodes are the backbone of Mynta's advanced features like InstantSend (near-instant transactions) and ChainLocks (protection against attacks).

---

## Why Run a Masternode?

### Earn Passive Income

Masternode operators receive **45% of every block reward**. With blocks generated every minute, this creates a steady stream of income distributed among all active masternodes.

### Support the Network

Your masternode helps power:

| Feature | What It Does |
|---------|--------------|
| **InstantSend** | Confirms transactions in seconds instead of minutes |
| **ChainLocks** | Protects against 51% attacks and double-spending |
| **Network Stability** | Provides reliable infrastructure for the ecosystem |

### Long-Term Investment

By running a masternode, you're:
- Accumulating more MYNTA over time
- Participating in network governance (future)
- Building stake in a growing ecosystem

---

## How Do Masternodes Work?

### The Basic Concept

1. **Lock Collateral**: You lock 100,000 MYNTA as collateral (it stays in your wallet)
2. **Run a Server**: You operate a server that stays online 24/7
3. **Get Paid**: The network pays you automatically for your service

### The Payment System

Mynta uses a **deterministic payment system**, meaning:

- Payments are 100% transparent and on-chain
- Every masternode gets paid in rotation
- No randomness or luck involved
- You can predict exactly when your next payment will come

```
Block Reward Distribution:
┌────────────────────────────────────────┐
│           5,000 MYNTA per block        │
├────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌────────┐ │
│  │  Miner  │  │Masternode│ │  Dev   │ │
│  │   50%   │  │   45%   │  │  5%   │  │
│  │ 2,500   │  │ 2,250   │  │ 250   │  │
│  └─────────┘  └─────────┘  └────────┘ │
└────────────────────────────────────────┘
```

### Staying Active

Your masternode must remain online and responsive. The network monitors masternodes through a "Proof of Service" (PoSe) system:

- **Good Behavior**: Stay online, respond to network requests = Get paid
- **Bad Behavior**: Go offline or miss duties = Accumulate penalty points
- **100 Penalty Points**: Temporarily banned (no payments until fixed)

Don't worry—occasional brief outages are tolerated. The system is designed to catch consistently unreliable nodes, not punish minor hiccups.

---

## Masternode Economics

### Core Numbers

| Parameter | Value |
|-----------|-------|
| **Collateral Required** | 100,000 MYNTA |
| **Block Reward** | 5,000 MYNTA |
| **Block Time** | 1 minute |
| **Masternode Share** | 45% (2,250 MYNTA per block) |
| **Blocks Per Day** | 1,440 |
| **Daily MN Reward Pool** | 3,240,000 MYNTA |

### How Payments Work

The entire masternode network shares the daily reward pool equally:

```
Your Daily Earnings = 3,240,000 MYNTA ÷ (Number of Active Masternodes)
```

### Payment Frequency

Since masternodes are paid in rotation:

```
Time Between Payments ≈ (Number of Masternodes) minutes
```

**Example**: With 500 masternodes on the network, you'd receive a payment approximately every 500 minutes (~8.3 hours), receiving 2,250 MYNTA each time.

---

## Earning Potential Calculator

Your earnings depend entirely on how many masternodes are active on the network. Here's a breakdown:

### Daily Earnings by Network Size

| Active Masternodes | Your Daily MYNTA | Payment Frequency |
|--------------------|------------------|-------------------|
| 100 | 32,400 | ~1.7 hours |
| 250 | 12,960 | ~4.2 hours |
| 500 | 6,480 | ~8.3 hours |
| 1,000 | 3,240 | ~16.7 hours |
| 2,500 | 1,296 | ~1.7 days |
| 5,000 | 648 | ~3.5 days |
| 10,000 | 324 | ~7 days |

### Monthly and Yearly Projections

| Active MNs | Monthly MYNTA | Yearly MYNTA | Yearly ROI* |
|------------|---------------|--------------|-------------|
| 100 | 972,000 | 11,826,000 | 11,826% |
| 250 | 388,800 | 4,730,400 | 4,730% |
| 500 | 194,400 | 2,365,200 | 2,365% |
| 1,000 | 97,200 | 1,182,600 | 1,183% |
| 2,500 | 38,880 | 473,040 | 473% |
| 5,000 | 19,440 | 236,520 | 237% |
| 10,000 | 9,720 | 118,260 | 118% |

*ROI = Return on Investment based on 100,000 MYNTA collateral

### Understanding the Economics

**Early Advantage**: When the network is young with fewer masternodes, returns are exceptionally high. As more masternodes join, individual rewards decrease but the network becomes more robust.

**Market Dynamics**: High returns attract new masternode operators, which decreases individual returns until an equilibrium is reached where the ROI matches what operators consider worthwhile.

**Halving Events**: Block rewards halve approximately every 4 years (every 2.1 million blocks). After the first halving, masternode rewards will be 1,125 MYNTA per block instead of 2,250.

### Break-Even Analysis

Your main ongoing cost is server hosting (typically $5-50/month depending on provider). Even at 10,000 masternodes with conservative estimates, your daily earnings in MYNTA significantly exceed hosting costs in most scenarios.

---

## Getting Started

### What You'll Need

**1. The Collateral**
- Exactly 100,000 MYNTA in a wallet you control
- Must be in a single transaction output (not spread across multiple)
- Requires 15 confirmations before your masternode activates

**2. A Server (VPS)**

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| CPU | 2 cores | 4+ cores |
| RAM | 4 GB | 8+ GB |
| Storage | 50 GB SSD | 100+ GB NVMe |
| Network | 100 Mbps | 1 Gbps |
| IP Address | Static, public | Static, public |
| Uptime | 95%+ | 99%+ |

Popular VPS providers: DigitalOcean, Vultr, Linode, Hetzner, Contabo

**3. Basic Technical Knowledge**
- Ability to connect to a Linux server via SSH
- Comfort with command-line operations
- Or: Use a masternode hosting service (they handle the technical parts)

### The Setup Process (Overview)

1. **Acquire MYNTA**: Purchase or mine 100,000 MYNTA
2. **Set Up Your Wallet**: Install Mynta wallet and transfer your MYNTA
3. **Create Collateral**: Send exactly 100,000 MYNTA to yourself (creates the collateral transaction)
4. **Wait for Confirmations**: 15 blocks (~15 minutes)
5. **Set Up Your VPS**: Rent a server and install Mynta Core
6. **Generate Keys**: Create your operator and voting keys
7. **Register**: Submit your masternode registration transaction
8. **Start Earning**: Your masternode begins participating in payment rotation

For detailed technical instructions, see the [Operator Guide](../mynta-core/doc/masternode/OPERATOR_GUIDE.md).

---

## Costs and Considerations

### Upfront Costs

| Item | Cost |
|------|------|
| 100,000 MYNTA Collateral | Market price |
| Transaction fees for setup | Minimal (< 1 MYNTA) |

### Ongoing Costs

| Item | Typical Cost |
|------|--------------|
| VPS Hosting | $5-50/month |
| Your Time | A few hours/month for monitoring |

### Risk Factors

**Market Risk**
- MYNTA price can go up or down
- Your collateral value fluctuates with the market
- Rewards are in MYNTA, not fiat currency

**Technical Risk**
- Server downtime affects your PoSe score
- Prolonged outages = missed payments
- Security breaches could compromise your operator keys

**Network Risk**
- Protocol changes could affect masternode economics
- Governance decisions may impact reward structures

### Collateral Safety

**Good News**: Your 100,000 MYNTA collateral never leaves your wallet. It stays in an address you control with keys only you possess. The masternode system only requires proof that you own the collateral—it doesn't lock it in a smart contract or send it anywhere.

**Important**: If you spend your collateral (even part of it), your masternode is automatically deactivated. You can re-register by creating new collateral.

---

## Frequently Asked Questions

### General Questions

**Q: Is my collateral locked?**

A: No. Your collateral stays in your own wallet and remains spendable at all times. However, if you spend it, your masternode becomes inactive. Think of it as "reserved" rather than "locked."

**Q: How soon will I receive my first payment?**

A: After registration and 15 confirmations, your masternode enters the payment queue. Your first payment arrives after one full cycle through all masternodes (time depends on total masternode count).

**Q: Can I run multiple masternodes?**

A: Yes! Each masternode requires its own 100,000 MYNTA collateral, unique IP address, and server (or dedicated resources on a server). There's no limit to how many you can operate.

**Q: What happens if my server goes down?**

A: Brief outages are tolerated. Extended downtime accumulates PoSe penalty points. At 100 points, your masternode is temporarily banned from payments. Once you fix the issue and your server proves it's online, the ban lifts.

### Financial Questions

**Q: How much can I really make?**

A: It depends on the number of active masternodes and the MYNTA market price. See the [Earning Potential Calculator](#earning-potential-calculator) for specific numbers. Early participants typically earn more because there are fewer masternodes sharing the reward pool.

**Q: When does the first halving happen?**

A: Block rewards halve every 2,100,000 blocks, which takes approximately 4 years at 1-minute block times. The first halving will reduce the masternode reward from 2,250 MYNTA to 1,125 MYNTA per block.

**Q: Do I pay taxes on masternode earnings?**

A: Tax treatment varies by jurisdiction. In most countries, masternode rewards are taxable income. Consult a tax professional familiar with cryptocurrency.

### Technical Questions

**Q: What if I lose my operator keys?**

A: You can update your masternode with new operator keys using an update transaction signed by your owner key. Always keep your owner key (cold wallet) secure—it's your ultimate backup.

**Q: Can I run a masternode at home?**

A: Technically yes, if you have a static public IP and can keep a computer running 24/7. However, most operators use VPS providers for reliability and to avoid home network complications.

**Q: Do I need to keep my wallet open?**

A: No. Your collateral wallet can be offline after registration. The VPS runs the masternode software independently. Your wallet only needs to be online for receiving payments (which happen automatically to your payout address).

**Q: What's the difference between owner, operator, and voting keys?**

| Key | Purpose | Storage |
|-----|---------|---------|
| **Owner** | Ultimate control, signs registration | Cold wallet (most secure) |
| **Operator** | Runs the masternode, signs service messages | VPS (hot wallet) |
| **Voting** | Participates in governance | Can be same as owner or separate |

This separation lets you run a masternode on a VPS without exposing your ownership keys to online risks.

### Troubleshooting

**Q: My masternode shows as "POSE_BANNED"—what do I do?**

A: Fix whatever caused the outage (usually server issues), ensure your masternode is running and synced, then submit a service update transaction. This proves you're back online and clears the ban.

**Q: I see "bad-protx-collateral-amount"—what's wrong?**

A: Your collateral transaction doesn't contain exactly 100,000 MYNTA. Create a new transaction with the precise amount (not 99,999, not 100,001—exactly 100,000).

**Q: Payments seem inconsistent—is something wrong?**

A: Payment timing depends on total masternode count, which changes as nodes join/leave. The actual amount per payment stays constant (2,250 MYNTA), but the time between payments varies.

---

## Summary

Running a Mynta masternode is a way to earn passive income while supporting a decentralized network. Here's the quick rundown:

| What You Provide | What You Get |
|------------------|--------------|
| 100,000 MYNTA collateral | 45% of block rewards, shared among all masternodes |
| A server running 24/7 | Automated payments every time it's your turn |
| Basic maintenance | Participation in network governance |

**The Bottom Line**: Masternode operation rewards those who commit resources to securing and enhancing the network. Returns are highest when participation is low and stabilize as the network matures.

---

## Next Steps

1. **Research**: Understand the current MYNTA market and masternode count
2. **Calculate**: Use the earnings table to estimate your potential returns
3. **Plan**: Decide on VPS provider and budget for ongoing costs
4. **Acquire**: Get your 100,000 MYNTA collateral
5. **Set Up**: Follow the [Operator Guide](../mynta-core/doc/masternode/OPERATOR_GUIDE.md) for technical setup

---

## Resources

- [Technical Operator Guide](../mynta-core/doc/masternode/OPERATOR_GUIDE.md) - Detailed setup instructions
- [Masternode Architecture](architecture/masternodes.md) - How the system works technically
- [MIP-001: Deterministic Masternodes](../mynta-core/doc/masternode/MIP-001-DETERMINISTIC-MASTERNODES.md) - The specification

---

*Last Updated: January 2026*
