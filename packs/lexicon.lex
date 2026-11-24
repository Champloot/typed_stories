# Lexicon: every intent is one meaning with many ways to say it.
# Format: see FORMAT.md
#   1: plain words (10 style points)
#   2: good words  (30)
#   3: fancy words (60)
# hint: what the answer means. The game shows only hints ("agree / refuse"),
#       never the phrases. Typing the hint itself also works, as a plain answer.
# Apostrophes and punctuation are ignored: "I'm good!" is the same as "im good".

# ---------------------------------------------------------------- general

intent: yes
hint: agree
1: yes | yeah | yea | yep | yup | ya | y | ok | okay | sure
2: aye | of course | sure thing | why not | yes please | alright | all right | please | go on | sounds good | fine
3: absolutely | certainly | by all means | I would be delighted | that would be lovely | most kind | much obliged | splendid | indubitably | with pleasure | gladly
3: verily | forsooth | it shall be done | I accept, my liege | affirmative | acknowledged

intent: no
hint: refuse
1: no | nope | nah | n
2: nay | negative
2: no thanks | no thank you | not really | I'm good | not today | pass | I will pass | I'll pass | nope nope
3: nay, my liege | I think not, sire | request denied | thanks but no thanks | I must politely decline | I shall decline | not on your life | absolutely not | I think not | over my dead body | thank you kindly, but no

intent: wait
hint: wait
1: wait | later | not yet | leave it
2: hold on | let it be | give it a minute | one sec | one second | in a bit
3: patience | all in good time | good things take time | I can wait | haste makes waste

intent: run
hint: hurry
1: run | go | sprint | dash | rush
2: make a run for it | full speed | I can make it | let's go | go go go
3: now or never | like the wind | fortune favors the bold | yolo | to glory

intent: left
hint: go left
1: left | go left | turn left | l
2: to the left | left please | hang a left | take a left
3: port | port side | leftward ho | the left path it is

intent: right
hint: go right
1: right | go right | turn right | r
2: to the right | right please | hang a right | take a right
3: starboard | rightward ho | the right path it is | follow the sign

# ---------------------------------------------------------------- morning

intent: snooze
hint: sleep more
1: snooze | sleep | not yet
2: five more minutes | 5 more minutes | ten more minutes | let me sleep | go away
3: the world can wait | wake me next week | I refuse | sleep is life | it is the weekend

intent: get_up
hint: get up
1: get up | up | wake up | ok | okay
2: rise and shine | I'm up | coming | fine | coffee
3: seize the day | carpe diem | rise like a phoenix | another day another dollar

intent: stove_off
hint: turn off the stove
1: off | turn off | stop | no
2: turn it off | heat off | kill it | save it
3: not on my watch | abort | save the milk | rescue the milk

intent: open
hint: open the door
1: open | open it | open the door | come in | hello
2: coming | just a moment | who is it | one moment | hi
3: do come in | enter | welcome | who goes there

intent: ignore
hint: ignore it
1: ignore | ignore it | nothing | shh
2: pretend I'm not home | stay quiet | hide | not home | silence
3: nobody is home | I am not here | ninja mode | I do not exist

# ---------------------------------------------------------------- commute

intent: hold
hint: hold the doors
1: hold | hold it | hold the door | yes
2: hang on | come on in | I'll wait | plenty of room | after you
3: climb aboard | all aboard | the more the merrier | welcome aboard

intent: close
hint: let them close
1: close | shut | close it | no | sorry
2: close the doors | not today | next one | full
3: the lift waits for no one | so long | farewell | see you never

# ---------------------------------------------------------------- work

intent: busy
hint: put it off
1: busy | I'm busy | not now | later | no
2: can it wait | in a meeting | after lunch | swamped | tomorrow | monday
3: my calendar disagrees | I am but a ghost | out of office | I am deeply occupied

intent: sing
hint: sing along
1: happy birthday | sing | hbd | happy bday | la la la
2: many happy returns | happy birthday to you | cheers
3: for she's a jolly good fellow | joyeux anniversaire | felicitations

intent: eat_cake
hint: go for the cake
1: cake | eat cake | eat | the cake
2: cake first | dessert | slice please | gimme cake | give me cake
3: let them eat cake | cake is the answer | the cake is mine

intent: water
hint: add water
1: water | add water | fill | refill
2: fill the tank | fill it | top it up | more water
3: hydrate the beast | quench its thirst | let it drink

intent: kick
hint: kick it
1: kick | hit | hit it | kick it
2: whack it | smack it | bump it | give it a kick
3: percussive maintenance | the ancient way | with feeling

# ---------------------------------------------------------------- walk

intent: drop_it
hint: make it drop it
1: drop it | drop | no | stop | leave it
2: spit it out | bad dog | give | let go
3: unhand that | release it at once | absolutely not

intent: allow_eat
hint: let it eat
1: bon appetit | let it | ok | enjoy | whatever
2: eat up | good boy | chew away | go ahead
3: you do you | who am I to judge | let him cook | dinner is served

intent: allow_pet
hint: allow
1: go ahead | pet | pet him | yes
2: he loves it | be my guest | go for it | sure go ahead
3: behold the good boy | he accepts tribute

intent: rude
hint: send him away
1: fuck off | go away | back off | get lost
2: piss off | buzz off | scram | shoo | not my dog | hands off
3: kindly go away | begone | unhand my dog | I think you should leave

intent: heel
hint: keep the dog close
1: heel | stay | sit | no | stop
2: down | wait | leave it | not today | easy
3: stand down | hold the line | steady

intent: let_go
hint: let the dog run
1: let go | go | get it | fetch
2: go get it | go on | release | free | sic em
3: unleash the beast | fly my pretty | release the hound

# ---------------------------------------------------------------- store

intent: aisle4
hint: aisle 4
1: aisle four | aisle 4 | four | 4
2: number four | the fourth one | fourth
3: aisle the fourth | four it is

intent: aisle7
hint: aisle 7
1: aisle seven | aisle 7 | seven | 7
2: number seven | the seventh one | seventh
3: follow the note | trust the note | seven it is

intent: buy
hint: buy it
1: two please | two | buy | take | yes
2: give me two | I'll take two | wrap it up
3: I live dangerously | one for me and one for regret

intent: lift_bag
hint: move your bag
1: lift bag | lift | lift it | move bag | move it
2: pick it up | take the bag | lift the bag | grab the bag
3: behold my bag | the bag is mine

intent: call_staff
hint: call staff
1: call staff | staff | help | call help
2: call for help | assistance | help please | excuse me
3: I need an adult | summon a human | send help

# ---------------------------------------------------------------- evening

intent: answer
hint: pick up
1: answer | pick up | hello | hi mom | yes
2: hi | hey mom | hello mom | mom
3: hello mother | greetings mother | your favorite child speaks

intent: decline
hint: decline
1: decline | reject | not now | later | no
2: call back later | I'll call back | busy | ignore
3: not now mother | the shower calls | another time

intent: pizza
hint: suggest pizza
1: pizza | pizza please | pepperoni
2: margherita | pizza place | the usual
3: pizza is love | pizza is life

intent: anything
hint: let him choose
1: anything | whatever | you choose | up to you
2: surprise me | your call | dealer's choice | I trust you
3: lead the way | I am in your hands | take me anywhere

# ---------------------------------------------------------------- consequences

intent: thank
hint: thank him
1: thanks | thank you | thx | ty
2: thanks a lot | much appreciated | you're a star | cheers mate
3: my hero | you are a gentleman and a scholar | eternally grateful | bless you

intent: apologize
hint: apologize
1: sorry | my bad | oops
2: I'm sorry | sorry about that | my apologies | won't happen again | I apologize
3: I deeply apologize | forgive me | mea culpa | I was a monster

intent: excuse
hint: make an excuse
1: traffic | bus | train | the bus
2: the bus was late | traffic was crazy | my alarm broke | doctor
3: my dog ate my alarm | time is a construct | I was here in spirit

intent: air_out
hint: air the flat
1: open the window | window | air | open windows
2: let some air in | fresh air | open everything | air it out
3: let the wind take it | begone foul smell | operation fresh air

intent: mask_smell
hint: cover the smell
1: candle | spray | perfume | air freshener
2: light a candle | cover it | use spray
3: fight smell with smell | vanilla will save us

intent: eat_it
hint: eat it
1: eat | eat it | yum | dinner
2: one bite | just a little | why not | bon appetit
3: fortune favors the hungry | what could go wrong | a feast

intent: throw_away
hint: throw it away
1: throw away | bin | trash | throw it out
2: into the bin | not worth it | goodbye sausage | toss it
3: rest in peace | to the bin with thee | it had a good run

intent: pay
hint: pay the bill
1: pay | ok | fine | pay it
2: here you go | card please | take my money
3: worth every penny | anything for my dog | the dog is priceless

intent: haggle
hint: haggle
1: discount | cheaper | too much | no
2: can you do better | any discount | that's a lot | that is too much
3: let us negotiate | I know a guy | what if I pay in compliments

intent: let_in
hint: let her in
1: come in | open | ok | yes
2: hi mom | come on in | of course
3: mother dearest | soup time | welcome home mom

intent: pretend_asleep
hint: pretend to sleep
1: sleep | shh | snore | zzz
2: I'm asleep | nobody home | lights off
3: I am but a dream | play dead | the house is empty

# ---------------------------------------------------------------- the knight

intent: fight
hint: fight
1: fight | attack | strike | stab | slash | sword
2: draw my sword | have at you | en garde | to arms | fight me
3: smite thee | for the king | have at thee, knave | taste my steel | by my blade

intent: talk
hint: talk
1: talk | speak | chat | hello | hi
2: let's talk | can we talk | parley | how are you | what's wrong
3: let us parley | pray, speak | tell me your sorrows | words, not swords | I come in peace

intent: flee
hint: retreat
1: flee | run | run away | retreat | escape
2: tactical retreat | fall back | back off | I'm out
3: discretion is the better part of valor | live to fight another day | brave sir robin

intent: share
hint: share your bread
1: share | here | take it | bread | have some
2: here you go | take some bread | have my bread | share my bread
3: break bread | what's mine is yours | a crust for a friend | charity

intent: help
hint: help it
1: help | help it | thorn
2: pull the thorn | let me help | easy now | good boy
3: let me see that paw | fear not | hold still, friend

intent: drink
hint: have a drink
1: drink | cheers | bottoms up | gulp
2: one please | pour me one | I'll drink | to your health
3: down the hatch | skol | fill my tankard | hail and drink

intent: swim
hint: swim across
1: swim | wade | swim across | walk
2: I'll swim | across | through the water | jump in
3: I fear no river | like a fish | the river is my friend

intent: toll
hint: pay the toll
1: pay | coin | toll | gold
2: pay the toll | take a coin | here is a coin | take my gold | fine, here
3: a coin for your trouble | keep the change | fair is fair

intent: bow
hint: bow
1: bow | kneel | bow down
2: I bow | my lord | your majesty | I remember
3: I remember you, wise one | at your service | your humble servant

# ---------------------------------------------------------------- unit R-7

intent: brew
hint: make coffee
1: coffee | brew | make coffee | yes
2: one coffee coming up | brewing | espresso | double shot | on it
3: initiating caffeine protocol | your fuel, sir | black as my circuits

intent: tea
hint: make tea instead
1: tea | no | no coffee | tea instead
2: make tea | chamomile | have some tea | not tonight
3: sleep protocol engaged | herbal intervention | you need rest, human

intent: click
hint: click the link
1: click | click it | open | open it | link
2: let's see | check it | click the link | follow the link
3: what could go wrong | curiosity | I trust the internet

intent: report
hint: report it
1: report | spam | delete | block
2: report it | mark as spam | report as phishing | delete it | nice try
3: forward to security | not today, scammer | phishing detected

intent: lie
hint: tick the box
1: tick | tick it | yes | lie | click
2: tick the box | I am human | I am not a machine | totally human | just a human
3: I bleed and I cry | as human as they come | my favorite food is food

intent: honest
hint: tell the truth
1: no | truth | honest | the truth
2: tell the truth | be honest | I am a machine | I am a bot | beep boop
3: I cannot lie | honesty is my protocol | my circuits do not lie

intent: fix
hint: fix it
1: fix | fix it | repair | open it
2: let me fix it | clear the jam | debug | take a look
3: diagnostics mode | gently, gently | I speak printer

intent: charge
hint: plug in
1: charge | plug in | plug | power | yes
2: plug me in | recharge | top up | juice
3: sweet, sweet electrons | feed me volts | ah, power

intent: wave
hint: wave
1: wave | hi | hello | wave back
2: hello there | good morning | waving | say hi
3: greetings, small human | have a nice day, citizen | beep beep, friend

intent: shoo
hint: shoo it
1: shoo | go away | away | scram
2: shoo shoo | off you go | get off | fly away
3: begone, foul bird | not on my sensors | evacuate my head
