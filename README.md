# TrackmaniaPathFinder

Program made to help find reroutes in Trackmania maps. The typical workflow for finding a reroute is:

1. Find all of the possible ways you can get from each CP to every other CP and write down in a spreadsheet the estimated time for each such connection.
2. Use this program to find all of the fastests possible routes that use those connections
3. Among found routes choose the best one.

The following sections explain how to use this program. I decided to write this more as a practical guide where I give some ideas on the general workflow of finding a reroute, rather than more technical specification. I do include some information on implementation details at the end for those who are interested.

If you have any questions / feature requests the best way is to contact me through discord [@isfoo](https://discordapp.com/users/552077071333982219 "@isfoo")

## Program screenshot

![screenshot](https://github.com/isfoo/TrackmaniaPathFinder/assets/128239594/798a383f-077e-44cf-8c76-f9702734338d)

## Input spreadsheet

First you need to create spreadsheet containing N by N cells, where N is the number of CPs + 1 for the finish. So if I have map that has Start, CP1, CP2, CP3, CP4, Finish I have 5x5 table:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | 999       | 999     | 999     | 999     | 999      |
| **CP2**     | 999       | 999     | 999     | 999     | 999      |
| **CP3**     | 999       | **999**     | 999     | 999     | 999      |
| **CP4**     | 999       | 999     | 999     | 999     | 999      |
| **Finish**  | 999       | 999     | 999     | 999     | 999      |

Where the row specifies from where Im going and the column specifies the destination. So for example in the above table the bolded cell will contain information about how long does it take to go from CP1 to CP3.

Initially the table should be filled with some big number, like that 999 in this example. This will mean that those connections takes 999 seconds. This arbitrary big value will be used to specify that given connection is not possible / should not be considered.

We can start filling in the table with the data we collected. Notice that the diagonal represents the normal route - going from Start to CP1, from CP1 to CP2, etc. Lets fill those in:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | **8.8**      | 999     | 999     | 999     | 999      |
| **CP2**     | 999       | **15**     | 999     | 999     | 999      |
| **CP3**     | 999       | 999     | **12.5**    | 999     | 999      |
| **CP4**     | 999       | 999     | 999     | **16.3**    | 999      |
| **Finish**  | 999       | 999     | 999     | 999     | **25**       |

Now we can fill the rest. Of course you only need to fill as much as you want or can - after all some connections might not even be possible. 

Note that in practice you don't have to exactly measure the time before you input it to the table - you can initially fill in good guesses about how long some connections take and only after you find the best possible routes you can go back and measure the connections used in those routes more accurately. This will save you time, because likely a lot of the connections will not even end up in any possible route, so measuring the time accurately will be a waste of time.

Let's assume we found and measured some connections and ended up with something like this:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | **8.8**       | 999     | 999     | **9**       | **8**        |
| **CP2**     | **13**        | **15**      | 999     | **5**       | 999      |
| **CP3**     | **15**        | 999     | **12.5**    | 999     | **7**        |
| **CP4**     | 999       | **2**       | **12**      | **16.3**    | 999      |
| **Finish**  | 999       | **12**      | 999     | **17**      | **25**       |

The last step is to save this table in CSV format. That means we want to have a file where each row of the table is a new line and values between columns are separated by something like commas, spaces, tabs, etc. Here's how the table could look like:

```
8.8	999	999	9	8
13	15	999	5	999
15	999	12.5	999	7
999	2	12	16.3	999
999	12	999	17	25
```

Currently the implementation only takes into account the first significant digit.

Note that the decimal part of a number uses decimal point and not a comma - that is important to keep in mind, because if you export your data from somewhere it might save those values with commas instead of decimal points and then the program won't be able to read the values correctly.

### Extended input spreadsheet - sequence dependence

Often how long it takes to go from CP `A` to CP `B` depends on from which way and at what speed you took CP `A` from. The program cannot understand entry angle or speed as it's highly track dependent, however this mostly depends on what CP was taken before CP `A`. 

That is imagine that going from CP `A` to CP `B` takes 10 seconds if we came from CP `D`, but same connection takes 13 seconds if we came from CP `E`, because in the first case we could carry the speed through CP `A` and in the second case we had to stop and turn around.

In order to input that data into spreadsheet we would write for connection from `A` to `B`: `10(D)13(E)` where D and E would be CP numbers. For start we use number 0. At the end of such chain it's also worth adding the time it would take to go from `A` to `B` when using standing-respawn. So if we say in that case it takes 14 seconds it would look like this: `10(D)13(E)14`. Additionally we can add multiple CPs in the brackets. Say the connection from `A` to `B` also takes 10 seconds when coming from CP `G`. In that case we have: `10(D,G)13(E)14`. 

Typically when using this system I found myself having one conditional cost for going with speed from one side, and second unconditional cost for turning around/standing respawn as it's mostly the same.

That this system is also useful for specifying if some connection is only possible from limited set of previous CPs. For example if connection from CP `A` to CP `B` takes 8 seconds, but can only be reasonably done with a lot of speed which can only be done if we came to CP `A` from CP `H` or `J` we would have in that case: `8(H,J)`.

You can see a full real example spreadsheet using this system [here](example%20input%20data/%5BRPG%5D%20Evergreen.csv)

## Basic program usage

First you need to download the program [TmPathFinder.exe](https://github.com/isfoo/TrackmaniaPathFinder/releases/latest/download/TmPathFinder.exe "TmPathFinder.exe").
It's a GUI application where you configure parameters and run the search for fastests routes.

Here's a basic explaination of the options:

**max connection time** - maximum time a connection can take to be considered part of the route. Generally you should set it to the arbitrarily big value you used in your input spreadsheet, however in practice as long as you don't expect 600 second connections you can just leave the default value.

**max route time** - maximum time a complete route from start to finish can take.

**max number of routes** - number of fastest routes you want to find. Unless you are working with a small number of connections you should not set it to an arbitrarily high value - this parameter plays a key role in how long the search process will take so you should set it to something reasonable.

**max search time** - maximum time in seconds you want to search for. This is mostly useful for heuristic algorithm since it will usually find most if not all top 100 solutions in the first ~10 seconds even for hard problems. Might need to increase that time for some problems - you have to experiment yourself.

**max repeat CPs to add** - how many repeat connections should be added to input spreadsheet. You can set it to arbitrarily high number to find all such connections. Best way to understand repeat CPs is by example. Say in my spreadsheet I wrote that going from CP1 to CP2 takes 12 seconds and going from CP2 to CP3 takes 5 seconds. I didn't write anything for CP1 to CP3 connection, because I couldn't find anything good. However actually if you go through CP2 you can get from CP1 to CP3 in 12+5=17 seconds. It might actually be the case that going through CP2 multiple times is worth it and required for the optimal route. Allowing this option it would find routes with that repeated CP2. Initially you might think it's rare for such connections to be useful, but that is not the case. Usually routes end up having key CPs like for example ones with easy access to reactor boost that let you go quickly to many points of the map that otherwise would take much longer. Also it means you don't have to manually input such connections in the input spreadsheet as this program will do it for you.

**turned off repeat CPs** - As nice as repeat CPs can be, sometime they might lead to routes that are too hard. Imagine I have CP that allows for great connections, but the set up for them is pretty difficult (say some hard reactor flight). Allowing the reoute to go through that CP more than once might be too hard, because with repeat connections you don't have the ability to respawn and try again quickly, but have to first get to that CP. In that case this allows you to set a list of CPs you don't want to be considered in repeat connections. The list is separated by commas/spaces.

**output data file** - path to the output file which after completing running the algorithm will contain sorted list of top **max number of routes** found.

**output append data file** - path to the output file that will contain information about all the runs you performed and all the candidate routes found in the order they were found. Typically you won't be interested in this file, it's mostly there so that you can easily come back to earlier results for example if you accidently overwrite the **output data file**. By default this field is empty which means no file will be created or written to.

**ring CPs** - Comma separated list of CP numbers that are rings. This will allow the program to also include routes where you standing-respawn after taking ring CP. In output such respawn is denoted with letter `R`. There are 3 limitations of this system. First it only finds routes, where the previously taken CP was not a ring. That is if you take normal CP then ring CP then another ring CP - it will not find the route that respawns after second ring CP to go back to the normal CP. Second limitation is performance. Using this for larger maps will significantly slow down searching for the routes. Lastly it can sometimes produce impossible routes when in combination with repeat CPs (which are explained below), however it's track dependent so it's impossible for the program to know if the route is possible or not. I won't explain when this problematic situtation happens, but if you will be unfortunate enough to stumble upon it you will easily understand it.

**input data file** - input file in CSV format with inserted connections as described in [Input spreadsheet](#input-spreadsheet) section. You can use **Find file** button to open windows explorer to pick the file or manually insert relative or absolute path to the file in the box to the right.

**Run exact algorithm** - After you set all parameters this is the button you want to press to find the fastests routes. Once it's completed you will see **Status** change to **Done** which means you can find sorted list of the solutions in the **output data file**. You can also see the solutions found in the GUI application. The solutions found are guaranteed to be optimal. Note that if status will change to **Timeout** it means the algorithm was not able to finish before **max route time** and in that case there are no guarantees on the quality of solutions found.

**Run heuristic algorithm** - see [Advanced program usage](#advanced-program-usage)

## Advanced program usage

If you followed the instructions and clicked **Run exact algorithm** and the program takes too long to complete here's the list of things you should try in listed order:

1. Decrease **max number of routes**. This is the main parameter that increases the search time. If you set it too high initially you should try decreasing this value.

2. Decrease **max route time**. You should set it something closer to the expected time the fastests routes should take. Of course you might not know that value, but if you set it to something too low the worst thing that can happen is the program will end without finding any route which will tell you that there are no possible routes with that time or lower and you can try increasing this value.

3. Switch to **run heuristic algorithm**. In that case feel free the set back **max number of routes** and **max route time** to whatever you want - it won't make a difference for heuristic algorithm. You should **NOT** expect to finish running the algorithm until the status changes to **Done**. Most of the time the algorithm will find most or all top 100 solutions in the first 10 seconds, but then it will continue trying possibly for hours/days or longer depending on the problem, often not finding anything new (as there might be nothing new to find). In practice you should look at 2 things. 1st how does **Candidates found** changes while running the algorithm. If it stays at the same value or barely changes then likely there is not much more to find. 2nd thing to look at is **Search progress**. You will see there `Completed X tries for K-opt`. You should run the program such that at the end you see in that text at least `K > 7` or `K = 7` and `X >= 100`. Note that this algorithm although in practice seems to work very well, unlike the **exact** algorithm even if it completes running (status **Done**) there are no guarantees on solution quality.

If you abused **ring CPs** and sequence dependent times it is possible that it will be harder to find optimal solutions. If you belive this might be happening try experimenting with that a bit:

1. If you set **ring CPs** you should try removing some/all of them. Note that in that case it will simply find all routes that don't use standing respawn. If you think using standing respawn is faster, usually you will be able to tell when it's best to do it. In that case you can create second version of the input spreadsheet that doesn't contain ring CPs and use this program to find routes between normal CPs and just take ring CPs when it's fastests.

2. If you used sequence dependent times in input spreadsheet try removing some of them.

## Output format

The routes are saved in a pretty straightforward format that's best seen through examples:

```115.0 [Start,8,2-5,1,6-7,Finish]```

This means the route takes 115 seconds and the order is: Start -> CP8 -> CP2 -> CP3 -> CP4 -> CP5 -> CP1 -> CP6 -> CP7 -> Finish

In case of repeat CPs the repeated CPs are shown in brackets ```()```. For example:

```154.3 [Start,8,5,9,3,(5),4,(9,3),1-2,6-7,Finish]```

This means the route is: Start -> CP8 -> CP5 -> CP9 -> CP3 ->(go through CP5)-> CP4 ->(go through CP9 then CP3)-> CP1 -> CP2 -> CP6 -> CP7 -> Finish

In case of ring CPs the standing respawn is shown as letter `R`. For example:

```74.0 [Start,7,4,R,1-3,R,5-6,Finish]```

This means CP4 and CP3 are ring checkpoints and the route is: Start -> CP7 -> CP4 ->(standing-respawn at CP7)-> CP1 -> CP2 -> CP3 ->(standing-respawn at CP2)-> CP5 -> CP6 -> Finish

## Examples of real data used for reroutes

| **Map** | **CP count** | **Spreadsheet** | **Spreadsheet creator** |
|-------------|-----------|---------|---------|
| [Macopolis RPG](https://trackmania.exchange/maps/112275/macopolis-rpg) | 25 | [spreadsheet](example%20input%20data/Macopolis%20RPG.csv) | isfoo |
| [[RPG] Evergreen](https://trackmania.exchange/maps/156959/rpg-evergreen) | 22 | [spreadsheet](example%20input%20data/%5BRPG%5D%20Evergreen.csv) | isfoo |
| [MTC - Castle of Confusion](https://tm.mania-exchange.com/maps/121329/mtc-castle-of-confusion) | 18 | [spreadsheet](example%20input%20data/MTC%20-%20Castle%20of%20Confusion.csv) | Lars_tm
| [Sobekite Eternal 2020](https://tm.mania-exchange.com/maps/182399/sobekite-eternal-2020) | 25 | [spreadsheet](example%20input%20data/Sobekite%20Eternal%202020.csv) | Lars_tm
| [[RPG] Biozone](https://trackmania.exchange/maps/85912/rpg-biozone) | 25 | [spreadsheet](example%20input%20data/%5BRPG%5D%20Biozone.csv) | Lars_tm
| [[RPG] Catsuya](https://trackmania.exchange/maps/95028/rpg-catsuya) | 109 | [spreadsheet](example%20input%20data/%5BRPG%5D%20Catsuya.csv) | Lars_tm |
| [World of Wampus 6](https://trackmania.exchange/maps/111213/world-of-wampus-6) | 100 | [spreadsheet](example%20input%20data/World%20of%20Wampus%206.csv) | Lars_tm |
| [World of Wampus 7](https://trackmania.exchange/maps/138791/world-of-wampus-7) | 100 | [spreadsheet](example%20input%20data/World%20of%20Wampus%207.csv) | Lars_tm |

Note that even the 100+ CP Lars spreadsheets are easily processed using exact algorithm.

## Implementation details

The problem solved by this program can be most accurately described as finding the top N lowest weight hamiltonian paths in directed graph with sequence dependent weights. Since problem of finding minimum weight hamiltonian path is a relaxed version of traveling salesman problem, generally in literature the algorithms are made and decsribed for the latter. Thus the algorithms used are made for finding top N solutions to SDATSP (Sequence Dependent Asymmetric Traveling Salesman Problem).

Although You will find online some efficient programs that solve TSP there are a couple of problems: 
1. It's hard to find a program that is fast and easy to use - most good solvers are using command line interface or are just an API in some programming language.
2. Many solvers don't natively support Asymmetric versions of TSP. For example [Concorde](https://www.math.uwaterloo.ca/tsp/concorde.html) which is consider one the best exact solvers doesn't support ATSP. Note that it's possible to convert ATSP to TSP problems by creating dummy nodes which is desribed well [here](https://www.linkedin.com/posts/andreas-beham-004279b_optimizing-asymmetric-travelling-salesperson-activity-7107809854215323648-FfuJ).
3. No support for control over repeat nodes (aka repeat CPs)
4. No ability to find top N solutions and not just a single one. This is a big one, because it's an important feature. There is a lot of hidden heuristic knowledge only the player generating solution has that can ultimatley decide which routes are truly the best, but to do that in practice one has to see a list of solutions. The factors are things like number of hard CPs in route and where the connections are (It's usually best to have hard connections first and easy connections last). I can also say from experience that once you have a list of routes you start to piece together a better picture of the map and it makes you realise which additional connections would be good and it leads you to finding new great connections.
5. No support for sequence dependence (that includes ring CPs)

Here's how the current implementations work:

### Exact algorithm

Of course it is best if we can fully solve the problem - that is finding actual top N solutions and not just N good solutions that may or may not be the actual top solutions. After all we don't want to miss some great routes.

In general first thing to note is that for exact algorithms you essentially have to check all possible paths. The differences between possible algorithms one could use are about how to try to skip checking as many paths as possible by proving they cannot possibly be fast enough. As an easy example say some connection between nodes `A` and `B` has assigned `cost=500`. If we already found a path with `cost=300` then we know for a fact that optimal solution cannot possibly contain `A->B` connection since the cost of that connection alone is higher than cost of current optimum known path so we can throw away all combinations that contain `A->B` connection.

This program uses **Branch and Bound** method with **Assignment relaxation** solved using **Hungarian method**. It's a mouthful, but it's nothing too complicated. 

**Branch and Bound** refers to the process of going through all possible routes where we try to check most promising routes first (that's the **Branch** part) and where we do this early stopping as desribed above (that's the **Bound** part).

**Assignment relaxation** is the bounding / early stopping method. **Assignment problem** is a simpler version of **ATSP** that can be solved quickly. Instead of finding the lowest cost `N+1` edges that form a single cycle we find lowest cost set of `N+1` edges such that each of the `N` nodes has exactly `1` out edge and exactly `1` in edge. It means that every single possible **ATSP** solution is also possible **Assignment problem** solution, however not all possible **Assignment problem** solutions are possible **ATSP** solutions since in the former we could have multiple cycles and not just one.

**Hungarian method** is an algorithm solving **Assignment problem**. A good simple explaination of this algorithm can be found [here](https://www.hungarianalgorithm.com/examplehungarianalgorithm.php)

So here's how the algorithm works. Note that it's only a rough description of general idea:

Input: `N` by `N` matrix with edge costs; `K` - number of best solutions to find.

1. Solve assignment problem using **Hungarian method** and save minimal cost of assignment as **LowerBound**.
2. If **LowerBound** is bigger than the cost of already found top `K` solutions then **return** (early stopping)
3. Otherwise choose the most promising edge `E` (one that most probably will be included in the solution). We do this by going through the edges used in optimal assignment and estimating how much **LowerBound** would increase if we were to remove this edge. The edge without which the **LowerBound** would increase the most is our chosen edge.
4. Fork into 2 possible recursive paths:
    1. Lock-in edge `E` - Remove all edges that cannot be part of the solution together with this edge. Go to step 1. 
    2. Remove edge `E` - Set the value of that edge to infinity. Go to step 1.

Since the algorithm tries for each edge to either include it at step 4.1 or exclude it at step 4.2 we are guaranteed to go through all possible combinations. However because at each step we calculate current **LowerBound** on minimal cost of the solution we will be able to relatively quickly realize there is no point in recursing futher and can skip tons of routes.

#### Sequence dependence

The program supports different weights depending on previous node in path. Note that this allows to model the speed/angle of entry by proxy, but also partially model ring CPs. Let's say I have a connection from CP 2 to CP 3 and from CP 2 to CP 4. If CP 3 is a ring CP this simply means that I can add a connection between CP 3 and CP 4 with the same cost as going from CP 2 to CP 4, as long as CP 2 was preceding CP 3, otherwise this connection doesn't exist.

The desribed above algorithm doesn't natively support such sequence dependent weights. What we do is we create a graph that for each conditional connection takes its lowest possible value. That is we assume each connection will happen to end up in a best case scenario. We use this new graph for the above algorithm. During the algorithm when some connection is removed we can update the graph weights if after removing a connection the minimum weight has increased.

### Heuristic algorithm

TODO: Old description is out of date after algorithm change